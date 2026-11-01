# Implementation Plan: Shading Contract v2

**Branch**: `026-shading-contract-v2` | **Date**: 2026-10-30 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/026-shading-contract-v2/spec.md`, with the eight
answers of its 2026-10-30 Clarifications session.

## Summary

Shading gets an OSL-shaped, versioned, test-doubled boundary (DB-018's service part, part of DB-006,
GitHub #32). There are five parts.

1. **`CRendererServices` v2** evolves in place and carries `kRendererServicesVersion{2, 0, 0}`. It
   gains four members:
   - `getAttribute` for `attribute()`, `option()` and `rendererinfo()`;
   - `getMessage` for `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()` and
     `lightsource()`;
   - `getMatrix` for every named space, `"object"`, `"shader"` and `"light"` included, with a time
     argument;
   - `tracer()`.

   One method changes its signature (`setOffendingObject`) and 28 are removed: the coordinate-system
   lookups, and the option getters whose only caller now asks `getAttribute`. The texture part is
   spec 027's, unchanged.
2. **The changes land inside the `CShadingContext` members** that both backends already call. The
   `.rslo` and `.slo` paths therefore issue the same requests with no emitter change. `lightsource()`
   is the exception: its backends differ today (Q2).
3. **`ITracer`** is Shading-owned and reached through `tracer()`. It is installed per frame and
   answered by a stateless Hiders adapter (`CHiderTracer`) over the unchanged `trace`/`traceEx`
   bodies. Every trace call Shading makes goes through two helpers (`traceCalls.h`).
4. **`ShaderGlobals`** describes a batch of shading points (structure of arrays, runtime count, grid,
   span or point) with an explicit `RayType`, `HitMode`, `TransportMode` and `ShadingPass`. A pure
   `classifyShading()` encodes the situation table derived from the code. Nothing in the render path
   uses either.
5. **Guardrails**:
   - a standalone double and a program with no `ribRender` (a load-time stub of 6 trapping symbols until Phase 8 and 4 after it, QC);
   - a trace replay recorded on the starting tree;
   - byte-identical images on macOS, and each platform against its own baseline;
   - DB-006 33 -> 31 lines, and DB-001 8 -> 5 lines (Q1).

6. **Findings resolved here (triage of 2026-10-31, `validation.md` §4)**: the type-safe copy of F1 (a `TypeMismatch` that cannot be
   copied safely is refused, supersedes Q4), the null current object of F2, the interpreter's `lightsource()` outside a loop of F3 and the
   accessor constants of F4. Seven other findings (O1 to O7) are out of scope and filed by the maintainer.

No image changes in any phase, with one exception: a shader that, on master, overflowed a destination, mixed a string with a number or
dereferenced a null object or light (FR-026 to FR-028) now refuses or answers not found. No scene of the suite is expected to reach one;
CP-d at every phase proves it.

## Technical Context

**Language/Version**: C++20 (`-Werror`), CMake >= 3.19.

**Primary Dependencies**: none new. `libRIBCommon`, `libshader_shading`, `libRIBRender`; LLVM only
for the existing JIT.

**Storage**: N/A; memory only, nothing persistent.

**Testing**: ctest.

- New label `shading_contract`: the standalone program, the rendered program, a `Situations` program of its own and the replay tests
  ([contracts/test-doubles.md](contracts/test-doubles.md)). The contract programs turn on in Phase 3, when the headers exist; each test
  stays `DISABLED` until its phase (Test gate, tasks.md).
- New label `shading_contract-record`: the replay recorder, which writes into the build tree only.
- Existing suites: `architecture`, `visual`, `shading_parity`, `texture_tile`, `^determinism$`, and
  the full suite (`-LE determinism-sweep`) at every phase.
- The determinism sweep runs once on each platform as the final phase's last task.

**Target Platform**: macOS (Apple Silicon, Apple Clang, vcpkg, deployment target 13.3) and Linux
(Ubuntu 24.04, GCC 13, `-Werror`). Linux is validated by the agent over ssh in a per-spec tree
(quickstart §4).

**Project Type**: renderer library and CLI.

**Domains touched**:

| Domain | What changes |
|---|---|
| Shading | New: `serviceTypes.h`, `tracer.h`, `traceCalls.{h,cpp}`, `shaderGlobals.h`, `situation.{h,cpp}`, `sloLoader.{h,cpp}` (moved, Q5). Edited: `rendererServices.h`, `shading.{h,cpp}` (message, attribute, option and matrix members; `objectHandle()`, `objectAttributes()`, `lightParameter()`; the observer hook; `gatherSample`), `execute.cpp`, `executeMisc.cpp`, `shaderFunctions.h` (`PARAMETEREXPR_PRE` and `LIGHTPARAMETEREXPR_PRE`, 32 `DEFFUNC` lines), `giFunctions.h` (2 reads via the accessor), `rslOps.cpp` (`op_lightsource_f`'s body only), `CMakeLists.txt` |
| Renderer core | `rendererServicesImpl.h`, `rendererShadingServices.cpp` (the four members, the name tables), `renderer.{h,cpp}` and `rendererStatics.cpp` (`frameTracer`, set and cleared), `rendererFiles.cpp` (the `.slo` loader leaves, Q5), `src/ri/CMakeLists.txt` |
| Hiders | New: `hiderTracer.{h,cpp}`. Edited (Q6): `irradianceDispatch.cpp`, its 2 trace calls |
| Consumers (tests) | New: `tests/shading_contract/`. Edited: the four whole-interface doubles (Q3), `tests/CMakeLists.txt` |

**Not edited**:

- `trace.cpp`, `bundles.{h,cpp}`, `photonMap.*`, `raytracer.*`, `reyes.*`, `photon.cpp`;
- every Geometry, Graphics State and Texturing source;
- the JIT emitter (`llvmEmitter.cpp`) and every `op_*` signature (two exceptions, decided by the maintainer: the two-space `ctransform` lowering and the new `op_ctransform_ss`, D8, Phase 4, 2026-10-31; and the triple `resultKind` for the color, normal and point overloads of the attribute-family and message builtins, D9, Phase 5, 2026-11-01);
- the plug-in ABI headers (`shadeop.h`, `dlo.h`, `implicit.h`, `dsply.h`);
- `CLAUDE.md`.

**Performance Goals**: no measurable change (SC-010). Each new service call replaces a field read once
per builtin call, not per lane. The tracer adds one virtual call per traced bundle or probe ray. No
lock is added on the hot path.

**Constraints**:

- byte-identical images and equal `MaxBlockAvgDiff` values on macOS in every phase; on Linux, against
  Linux's own baseline;
- no threshold widening, no reference re-render, no replay re-recording;
- the allowlist only shrinks;
- no task depends on the merge;
- `cpujob` for every build and test: at most `-j4` on macOS, `-j2` for the sweep and on Linux.

**Scale/Scope**: about 10 new source files, about 14 edited sources, 2 new test programs, a stub, the
doubles and 2 replay files.

**Measurements at the plan stage** (static; no build was run):

- **Interface**: 57 pure virtual methods plus the destructor. The spec's "56" is off by one
  ([research.md](research.md) R3).
- **Allowlist**: DB-001 8, DB-005 8, DB-006 33 (26 include, 7 link). Shading metric 0/0.
- **libshader imports (macOS)**: the 6 of DB-001. This is provisional, read from another worktree's
  build that matches master for these symbols (R10). Phase 1 re-measures it.
- **Call sites**: 25 coordinate-system call sites (textual calls to `findCoordinateSystem(` or `jitFindCoordinateSystem(` outside their declarations, definitions and comments: 17 in `shaderFunctions.h`, 1 each in `giFunctions.h` and `shaderOpcodes.h`, 3 in `shading.cpp`, 3 in `rslOps.cpp`) plus 2 forwarders inside `shading.cpp`, all through one member while shading (the spec's "about 30" counts mentions; Phase 1 re-counts, C1); at bind time `src/ri/render/init.cpp` expands the same macros through `#define findCoordinateSystem CRenderer::findCoordinateSystem` (line 60) into direct core calls, which stay (corrected 2026-10-31, #123 diagnosis; see tasks T086); 10 trace call sites in Shading,
  plus 2 in Hiders for `occlusion`/`indirectdiffuse`; the executor reads of the object's
  attributes (R5: re-measured at T008; the tree shows 22 code occurrences of `currentObject->attributes` in all, and the figure that matters is the 7 + 1 sites compiled into `execute.cpp`/`executeMisc.cpp`/`shaderFunctions.h`/`giFunctions.h`; a differing total is not a stop condition).
- **Disk**: `/Volumes/Projects` had 7.5 GiB free; a build directory is about 0.6 GiB.

## Constitution Check

*GATE: must pass before Phase 0 research; re-checked after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Clean code | PASS | Two hand-coded name tables become one service each. The coordinate-system resolution becomes one service. Trace calls go through one place. 28 dead or replaced interface methods are deleted. |
| II. C++20 / C17 | PASS | `enum class`, `inline constexpr`, a small class template, `final`, `override`. No extensions. |
| III. TDD (non-negotiable) | PASS (by plan order) | Phase 2's contract tests are red by build failure (quickstart §1). The replay tests are characterization tests, green by construction and shown red by mutation (Q7, spec 028's precedent). |
| IV. CLI | N/A | No CLI change. |
| V. Minimal dependencies | PASS | None added. |
| VI. Linux and macOS | PASS | Both platforms record their own baselines and are checked against them. Linux is checked by the agent over ssh (quickstart §4). |
| VII. Docs | PASS | Registry, allowlist header counts, DEVNOTES. `CLAUDE.md` only with the maintainer's explicit authorization, never its "Spec-kit agent rules". |
| VIII. Domain separation | PASS | Below. |

**Principle VIII — Domain Separation and Modular Boundaries** (every plan):

- [x] **Domains touched are named**: Shading, the renderer core, Hiders (one new adapter, and one file
      under Q6), and the tests. Graphics State, Geometry, Texturing, Scene Interface and Display
      Outputs are read, not edited.
- [x] **No new cross-domain dependency outside the table, and none on the core from a domain**:
  - **Added**:
    - Shading -> Graphics State (`execute.cpp` includes `attributes.h`);
    - Shading -> Infra (the new headers);
    - Hiders -> Shading (`hiderTracer.h` includes `tracer.h`/`shading.h`; by Q6,
      `irradianceDispatch.cpp` calls `libshader::traceRay`);
    - Core -> Shading and Core -> Hiders (the services implementation, the frame slot,
      `parseSloShader`).

    All of these are allowed. None of the new Shading headers includes a Hiders, Geometry or core
    header (FR-003, quickstart §3).
  - **Removed**: 2 Shading -> Geometry include edges (DB-006), and 3 Shading -> Hiders link edges
    (DB-001, Q1).
  - **The upward capability** (Shading asking for rays) is inverted through `ITracer`, owned by
    Shading and implemented by Hiders (rule 2). It is no longer bound at load time for
    `trace`/`traceEx`. The bundles and the photon map stay as registered load-time imports, which
    is spec 029's work.
- [x] **Interface changes specified**: [contracts/renderer-services-v2.md](contracts/renderer-services-v2.md),
      [contracts/tracer.md](contracts/tracer.md), [contracts/shader-globals.md](contracts/shader-globals.md),
      [contracts/test-doubles.md](contracts/test-doubles.md), [contracts/allowlist-end-state.md](contracts/allowlist-end-state.md),
      and [data-model.md](data-model.md).
- [x] **No hider-specific special case**: the services and the tracer name no hider.
      `classifyShading()` takes `rasterHider` as an input that describes the situation; it is not a
      branch in a shared path, and only tests call it.
- [x] **Plug-in interfaces**: none changed. The services and the tracer are internal C++, not a
      plug-in boundary (rule 4 does not apply; the version constant follows D22 and FR-001).
- [x] **Registered violations not extended; reductions noted**: DB-006 33 -> 31; DB-001 8 -> 5 (Q1);
      DB-018's service part is done and the entry stays Open. DB-005 is unchanged, and so are DB-007
      (the `.slo` loader moving into Shading does not change the core's choice of backend), DB-017
      and DB-019.

**Post-design re-check (after Phase 1)**: PASS, unchanged. The contracts add no edge beyond those
listed. One spec premise turned out false, that DB-001 loses no line. It was raised as Q1 and decided
on 2026-10-30, and the spec is amended.

## Project Structure

### Documentation (this feature)

```text
specs/026-shading-contract-v2/
├── spec.md, plan.md, research.md, data-model.md, quickstart.md
├── contracts/renderer-services-v2.md, tracer.md, shader-globals.md, test-doubles.md, allowlist-end-state.md
├── tools/                      # Phase 1: image_manifest.sh (copied from 028), baselines, archcheck and import reports
├── validation.md               # Phase 1 onward: red evidence, checkpoints, Linux results
├── checklists/requirements.md
└── tasks.md                    # /speckit-tasks (not created here)
```

### Source code touched

```text
src/libshader/shading/
├── rendererServices.h        # v2: version + history, getAttribute/getMessage/getMatrix/tracer, 28 removed, setOffendingObject(ObjectHandle)
├── serviceTypes.h            # NEW: handles, statuses, requests and answers
├── tracer.h                  # NEW: ITracer
├── traceCalls.{h,cpp}        # NEW: libshader::traceRay/traceBundle, the no-tracer default, the observer
├── shaderGlobals.h           # NEW: ShaderGlobals, Strided<T>, RayType, HitMode, TransportMode, ShadingPass, BatchShape
├── situation.{h,cpp}         # NEW: classifyShading() (tests only)
├── sloLoader.{h,cpp}         # MOVED from src/ri/render/rendererFiles.cpp:53-343 (Q5)
├── shading.{h,cpp}           # members through the services; objectHandle/objectAttributes/lightParameter; observer; gatherSample
├── execute.cpp, executeMisc.cpp  # no surface.h; the reads through objectAttributes(); the trace calls through traceCalls
├── shaderFunctions.h         # PARAMETEREXPR_PRE(accessor, expected), LIGHTPARAMETEREXPR_PRE through lightParameter()
├── giFunctions.h             # photonEstimator through objectAttributes()
├── rslOps.cpp                # op_lightsource_f's body through getMessage (Q2)
└── CMakeLists.txt            # + traceCalls.cpp, situation.cpp, sloLoader.cpp
src/ri/render/
├── rendererServicesImpl.h, rendererShadingServices.cpp   # the four members, the name tables, setOffendingObject(ObjectHandle)
├── renderer.{h,cpp}, rendererStatics.cpp                 # frameTracer: set in beginFrame, cleared in endFrame
└── rendererFiles.cpp                                     # calls libshader's parseSloShader
src/ri/hiders/
├── hiderTracer.{h,cpp}       # NEW: CHiderTracer
└── irradianceDispatch.cpp    # 2 trace calls through libshader::traceRay (Q6)
src/ri/CMakeLists.txt         # + hiders/hiderTracer.cpp in ri_sources; excluded from ribVector with the other hider units
tests/shading_contract/       # NEW: doubles, load_stub.cpp, the standalone and rendered programs, scenes, shaders, replay/{macos,linux}.txt
tests/shading_parity/{table_services,recording_services,recording_texture_system}.h   # the removed overrides deleted (Q3)
tests/CMakeLists.txt          # + add_subdirectory(shading_contract)
tests/architecture/allowlist.txt, (metrics-baseline.txt if the report differs)
DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md, DEVNOTES.md
```

**Structure decision**: the existing layout. Every new source sits in a directory that
`domain-map.txt` already classifies (`src/libshader/` is Shading, `src/ri/hiders/` is Hiders), so
the domain map does not change. Tests live under `tests/`, which `archcheck` does not analyse as a
domain. The standalone program compiles no `src/` file of its own: it links `state_only_sources`
(spec 025), so archcheck's "incomplete build tree" rule is not touched.

## Design

Details are in [research.md](research.md), and the interfaces in [contracts/](contracts/). In brief:

1. **Placement (R1)**: the interfaces are Shading's, in `src/libshader/shading/`. The core implements
   the services, and a Hiders adapter implements the tracer.
2. **Version (R2)**: one constant for the boundary (D22, Q9). Each implementer `static_assert`s the
   MAJOR it was written for.
3. **The 57 methods (R3)**: 28 unchanged, 1 replaced, 28 removed (Q3).
4. **`getAttribute` (R4)**: the request carries the expected type, and the answer is the raw stored
   value plus a status. Master's name tables move into the renderer's implementation unchanged.
   Shading refuses a `TypeMismatch` (destination untouched, 0, one warning) where the copy could not be made safely: a string against a number, or more floats than a vector or matrix destination holds. A smaller stored value is still copied as on master, and a float-class destination of unknown size is a filed residual (F1, FR-026, O7; supersedes Q4).
5. **Handles and the reads that stay (R5)**: `ObjectHandle`/`ShaderHandle` are opaque. Two
   out-of-line accessors in `shading.cpp` (`objectHandle()`, `objectAttributes()`) are what let
   `execute.cpp`/`executeMisc.cpp` drop `surface.h`. No cache in `CShadingState`.
6. **`ITracer` (R6)**: two methods mirror `trace(CRay *)`/`traceEx(CRayBundle *)`. It is installed per
   frame. Hiders running in one frame cannot overwrite each other: one hider class per frame, a
   stateless adapter, and a slot written only while no render thread exists. With no tracer, every
   ray misses and one warning is issued per kind.
7. **`getMessage` (R7)**: master's source mapping is kept, including `incident` -> Interior. The
   `getParameter()` call moves behind the service; the variable/global-index answer is unchanged.
   `lightsource()` keeps each backend's own behaviour (Q2).
8. **`getMatrix` (R8)**: it returns the kind as well as the matrices. The diagnostics stay in Shading,
   word for word. The time is 0 (Q14).
9. **No services (R9)**: the four new members have defaults (`Unavailable`/`nullptr`). Shading warns
   once per kind.
10. **The standalone program (R10)**: it links no `ribRender`. A trapping stub supplies 6 symbols until Phase 8 and 4 after it (QC), and
    the `.slo` loader moves into Shading (Q5). Bundle and irradiance-cache consumption is tested in a
    second, `ribRender`-linked program (Q6).
11. **`ShaderGlobals` (R11, R12)**: structure of arrays with `Strided<T>` fields and an explicit
    absence rule. `du`/`dv`, `rayWidth` and `dPdx`/`dPdy` are separate fields. The footprint is
    `CAttributes::shadingRate`, which is already √area.
12. **Situations (R13)**: a 14-row table derived from every `shade()`/`displace()` call site. It
    records 8 oddities and fixes none.
13. **SC-005 (R14)**: a single-threaded trace replay recorded on the starting tree through a
    null-in-production observer (Q7).

## Phases and order of work

Every phase from 3 on ends with quickstart §2's checkpoint on macOS. Every phase that changes `src/`
also ends with §4b's checkpoint on Linux. Each phase pauses for the maintainer.

| # | Phase | Content | Allowlist lines deleted first | Tests that turn green or guard |
|---|---|---|---|---|
| 1 | Setup and baselines | macOS §0 (manifest, diffs, noise, archcheck, imports, fixups). The `traceRequestObserver` hook at the 10 + 2 call sites (null in production), the recorder, and the macOS replay; the per-builtin mutations shown red; the manifest re-confirmed with the hook in; a determinism-sweep image manifest of the starting tree on each platform (B1). **Gate**: Linux §4a (baseline, archcheck, `nm -D`/`readelf`, replay) before Phase 2 | none | `ShadingContract_TraceReplay_*` green by construction |
| 2 | Tests first | `tests/shading_contract/`: `CContractServices`, `CRecordingTracer`, `CCannedTracer`, `load_stub.cpp`, `contract_forwarder.h`, the programs (standalone, rendered, and `Situations` on its own), scenes and shaders. Behind `OPENRENDER_SHADING_CONTRACT_TESTS` (OFF until Phase 3, QF); every test `DISABLED` until its phase; red evidence in a scratch build with `-DOPENRENDER_SHADING_CONTRACT_TESTS=ON`. The double carries a TEMPORARY block of overrides (QD) and the stub two TEMPORARY traps (QC) | none | all new tests red by build failure |
| 3 | Foundation | `serviceTypes.h`, `tracer.h`, `shaderGlobals.h`, `situation.{h,cpp}`, the version constant and history; the four new members with defaults (no caller yet); `objectHandle()` and the shader-handle helper (QG); `sloLoader` moved (Q5; the `OPENRENDER_HAVE_LLVM` block moves with its guard, B2); option ON once the headers exist; a JIT-off configure and compile of `libshader_shading` at the checkpoint (B9) | none | `StandaloneLink`, `StandaloneLinkLine`, `ShaderGlobalsDouble`, `DoubleGlobalsMatchRenderer` |
| 4 | US4: `getMatrix` | `CRendererServicesImpl::getMatrix`; `findCoordinateSystem`'s body; `findCoordinateSystem`/`WithType` removed with their overrides (the double's TEMPORARY block and the four doubles); a `getMatrix` forwarding line in the three wrappers (QA) | none | `Matrices{Interp,Jit}`, `NoServices_Matrix`, `RealAnswers*_Matrices` |
| 5 | US3: `getAttribute` | `attributes()`/`options()`/`rendererInfo()` through `getAttribute`; the name tables in the core; `PARAMETEREXPR_PRE`'s expected type; the 26 option members removed (Q3); the type-safe copy (F1), the null object (F2) and the accessor constants (F4); a `getAttribute` forwarding line in the wrappers (QA) | none | `Attributes{Interp,Jit}`, `NoServices_Attribute`, `RealAnswers*_Attributes`, **new:** `TypeMismatch_Attribute*`, `NullObject_Attributes*`, `MessageAccessors` |
| 6 | US3: `getMessage` | the five object sources; `lightParameter()`; `op_lightsource_f`'s body (Q2); the type check before the write (F1), `shadername(type)` null guard (F2), the interpreter's no-light guard (F3); a `getMessage` forwarding line in the wrappers (QA) | none | `Messages{Interp,Jit}` (lightsource's documented difference), `NoServices_Message`, `RealAnswers*_Messages`, **new:** `TypeMismatch_Message*`, `NullObject_Messages*`, `LightsourceNoLoop*` |
| 7 | DB-006 | `objectAttributes()` (`objectHandle()` came in Phase 3, QG); the 7 reads (+ 1 `setOffendingObject`) compiled into `execute.cpp`/`executeMisc.cpp` (research R5) through them, while `shading.cpp`'s own reads stay direct; `setOffendingObject(ObjectHandle)`; `surface.h` removed from `execute.cpp`/`executeMisc.cpp` | 2 DB-006 | CP-c (exactly 2 fewer lines); quickstart §3 greps |
| 8 | US6: tracer | `traceCalls`, `CHiderTracer`, `frameTracer`; the 10 Shading call sites, plus the 2 in `irradianceDispatch.cpp` (Q6); the observer moved into the helpers; the stub's two TEMPORARY traps deleted (6 -> 4 symbols, QC) and the table re-measured; a `tracer()` forwarding line in the wrappers (QA) | 3 DB-001 (Q1) | `TraceRequests*`, `GatherRequests*`, `CannedTrace*`, `NoTracer`, `NoServices_Tracer`, **`TraceReplay_*` unchanged** |
| 9 | US5: situations | trailing fields in the two spec-039 observer reports; `ShadingContract_Situations` | none | `Situations` |
| 10 | Guardrails, docs, Linux | registry (DB-001, DB-006, DB-018), allowlist header counts, metrics (if the report differs), DEVNOTES; the JIT-off build (§4c); SC-010 timing; Linux §4b; **last: the determinism sweep on macOS and on Linux, diffed against the Phase 1 sweep manifests** (§5) | none | CP-a to CP-d on both platforms; the sweep green on both |

Notes:

- **Phases 4 to 6 can run in any order.** Each is a separate service. They come before Phase 7, so
  that Phase 7's include removal finds only the reads of research R5 left.
- **Phase 8 comes after Phase 7**, so that the tracer phase's archcheck delta is only DB-001's.
- **Stop rule**: a CP-d difference, an unexpected `VIOLATION`, or a replay difference stops the phase
  for the maintainer.

## Test design (red first)

| Id | Test | Red on the starting tree because | Covers |
|---|---|---|---|
| T-L1 | `ShadingContract_StandaloneLink`, `_StandaloneLinkLine` | build failure (new headers); the link-line script finds no program | US1 AS1, FR-004, FR-024, SC-001 |
| T-A1 | `ShadingContract_Attributes{Interp,Jit}` | build failure | US3 AS1, FR-005, FR-007, SC-003 |
| T-M1 | `ShadingContract_Messages{Interp,Jit}` | build failure | US3 AS1, FR-006, FR-007, SC-003 |
| T-X1 | `ShadingContract_Matrices{Interp,Jit}` | build failure | US4 AS1-AS3, FR-008, FR-009, SC-004 |
| T-T1 | `ShadingContract_TraceRequests{Interp,Jit}` (standalone: the probe forms and `trace()`'s/`transmission()`'s bundle requests); `_GatherRequests{Interp,Jit}` (rendered: `gather()` constructs a `CGatherBundle`, which traps on the stub) | build failure | US6 AS1 (requests), FR-010, FR-011 |
| T-N1 | `ShadingContract_NoServices_{Matrix,Attribute,Message,Tracer}` (standalone; four tests, each green in the phase that routes its service, QB; the tracer one covers the probe forms, the bundle forms' miss path runs `postShade`, so it is T-T2's `_NoTracer`) | build failure | US1 AS3, FR-012, edge case "no services", FR-023(f) |
| T-G1 | `ShadingContract_ShaderGlobalsDouble` | build failure | US5 AS1, AS3, FR-015, FR-016 |
| T-R1 | `ShadingContract_RealAnswers{Interp,Jit}` | build failure | US3 AS2, US2 AS3, FR-022 |
| T-T2 | `ShadingContract_CannedTrace{Interp,Jit}`, `_NoTracer` (rendered: bundle forms, `gather`, `occlusion`/`indirectdiffuse`) | build failure | US6 AS1, AS3, FR-012 |
| T-D1 | `ShadingContract_DoubleGlobalsMatchRenderer` | build failure | the double's globals (R10) |
| T-S1 | `ShadingContract_Situations` | build failure | US5 AS2, FR-017, FR-018, SC-006 |
| T-P1 | `ShadingContract_TraceReplay_<builtin>` (8) | **characterization**: green by construction; red under a per-builtin mutation (Q7) | US6 AS2, FR-011, SC-005 |
| T-F1 | `ShadingContract_TypeMismatch_{Attribute,Message}{Interp,Jit}` (standalone, with a canary beside the destination) and `ShadingContract_TypeMismatchReal_{Attribute,Message}{Interp,Jit}` (rendered; the member called directly with a canary-padded buffer, each family in an executable of its own, B10; the attribute family includes a real `rendererinfo()` mismatch) | red at the Test gate of Phase 5 and 6: the canary is overwritten (standalone); for the direct-call executables, a build failure on the changed signature in Phase 5 (attribute family) and the overwritten canary in Phase 6 (message family, which compiles from Phase 5 because Phase 5 gives the five members their `QueryType` argument and ignores `expected` until Phase 6) | FR-026, SC-013, US3 AS4 |
| T-F2 | `ShadingContract_NullObject_{Attributes,Messages}{Interp,Jit}` (rendered, real services, `currentObject = nullptr`); `Messages` includes `shadername(type)` | red: crash or wrong answer | FR-027, SC-014, US3 AS5 |
| T-F3 | `ShadingContract_LightsourceNoLoop{Interp,Jit}` (standalone) and `ShadingContract_LightsourceNoLoopReal{Interp,Jit}` (rendered, real services; the interpreter crashes on master, the JIT pins its existing guard) | red: crash | FR-028, SC-014, US3 AS6 |
| T-F4 | `ShadingContract_MessageAccessors` (CMake script test over `shaderFunctions.h` and the two JIT members in `shading.cpp`) | red: the 8 `incident`/`opposite` lines and the 2 JIT members pass the crossed accessors | FR-029 |
| T-O1 | `ShadingContract_OffendingObject` (rendered, own executable; a script error reaches `setOffendingObject(ObjectHandle)` of the double with the object's handle) | build failure until Phase 7 | FR-003, Q11 |
| T-C1 | `ctest -L architecture` after each allowlist phase's deletion | exactly that phase's `VIOLATION` lines | FR-019, SC-008 |

Existing guards whose assertions do not change:

- the visual and parity suites; `ShadingParity_*` and `ShadingServices_*` (with Q3's override
  deletions);
- `LibShader_OpcodeCoverage`; `texture_tile`; `Visual_photon-parity{,-slo}`;
- `^determinism$`, and the sweep at the end.

## Risks

- **A name-table move that changes a cast**, for example `(float)` on an `int`. The real-answer
  tests T-R1 and the byte-identical manifest catch it. Each table entry is moved verbatim with its
  expression.
- **Shading copy semantics.** `PARAMETEREXPR_PRE`'s source selection and the varying-to-uniform guard
  must stay in Shading, applied to the service's answer. Mitigated by keeping the macros' structure
  and by T-M1 under both backends.
- **The stub on macOS.** Chained fixups bind every import at load, so a missing definer fails at
  start, not at use. The Phase 1 and Phase 8 measurements keep the stub table equal to the measured
  imports, and a missing symbol fails T-L1 loudly.
- **Linux binding.** If `libshader_shading.so` is `BIND_NOW`, the stub must define every undefined
  function too. `readelf -d` in §4a decides this, and the table may differ by platform (recorded,
  not tagged: the stub is test code).
- **`lightsource()` stays divergent** (Q2, decided). Making both backends issue identical requests
  would change the JIT's output, which FR-022 forbids. That needs its own output-changing spec.
- **Shared files with spec 037's worktree**: `allowlist.txt` and `DOMAIN_BOUNDARIES.md` are edited in
  disjoint places. No task depends on 037.
- **The type check refusing a legitimate query** (F1). The rule refuses only a string against a number and more floats than a vector or
  matrix destination holds; a smaller stored value and every float-class query are copied as on master, so `option("Format", fmt[3])` in
  `scene_state_probe.sl` still works. CP-d and the `Visual_*` scenes with the two shipped probes are the evidence; a difference stops the phase.
- **A wrapper silently losing a service** (QA). The forwarding lines are added in the phase that makes the member live, and every phase's
  checkpoint runs the six wrapper-installing tests of `tests/shading_parity` unchanged.
- **Known flaky tests** (memory `reference_ctest_flaky_tests.md`): a red run is re-run alone before
  it is written off. `ShadingParity_AmbientAccumulation` is a real heap bug.

## Open questions for the maintainer (with recommendations)

**Decided 2026-10-30**: the maintainer accepted the recommended answer to every question, Q1 to Q14. Each answer follows its question and is recorded in spec.md, Clarifications, "Session 2026-10-30 (plan-stage decisions)". The questions are kept as written at the plan stage.

None of these changes a reference-doc decision. Q1, Q2, Q3, Q6 and Q13 would change the spec's
wording.

What each answer unblocks:

| Question | Blocks |
|---|---|
| Q6 | Phase 1: the observer goes in at 10 or at 12 call sites |
| Q7 | Phase 1 |
| Q5 | Phase 3 |
| Q3 | Phases 4 and 5 |
| Q4 | Phase 5 |
| Q2 | Phase 6 |
| Q11 | Phase 7 |
| Q12 | Phase 7 |
| Q1 | Phase 8's allowlist deletions |

Q8, Q9, Q10, Q13 and Q14 are confirmations that block no phase.

- **Q1. DB-001 loses 3 lines, against FR-010/FR-020/the Assumptions.** Routing Shading's trace calls
  through a virtual `ITracer` means no Shading object references `trace(CRay *)`/`traceEx` any more.
  Archcheck then reports `allowlist.txt:15, 16, 21` as `STALE`, which must be deleted.
  **Recommendation**:
  - accept: delete the 3 lines, record DB-001 8 -> 5 (imports 6 -> 4) with spec 026's date, and keep
    DB-001 Open;
  - amend FR-010 ("claims no DB-001 line"), FR-020 ("DB-001 is not edited") and the Assumptions to say
    the trace imports go, while the bundles and the photon-map lookup stay for spec 029.

  The only alternative that keeps the lines is not routing the calls, which contradicts FR-010 and
  FR-011.

  **Decided 2026-10-30**: accepted. DB-001 loses the 3 lines, 8 -> 5 (imports 6 -> 4) and stays Open; FR-010, FR-020 and the Assumptions are amended.

- **Q2. `lightsource()` differs between the backends and reaches no `CShadingContext` member.**

  | | Interpreter | JIT (`op_lightsource_f`) |
  |---|---|---|
  | Light | the iterated light | `currentLightInstance`, the last light that ran |
  | Mutable values | read | ignored |
  | Types | every type | float only |
  | `result` | the found flag | the value |

  **Recommendation**:
  - both go through `getMessage` with source `Light`, each keeping its own light, `wantVariable` and
    result handling; the edits are a new member used by the macro and `op_lightsource_f`'s body, with
    no emitter or ABI change;
  - each backend's output stays unchanged (FR-022), and SC-003's "identical requests" holds for the
    other seven builtins;
  - record `lightsource` as a divergence, file an issue, and amend FR-007/SC-003 to name the
    exception.

  **Decided 2026-10-30**: accepted. Each backend keeps its own `lightsource()` output through `getMessage`; FR-007 and SC-003 are amended.

- **Q3. Where the option name table lives, and the removals.** If `option()` asks `getAttribute`, the
  25 option getters and `lookupUserOption` lose their one caller. `findCoordinateSystem` has no
  caller today, and `findCoordinateSystemWithType` loses its one. Removing these 28 methods means
  deleting their overrides in the four whole-interface test classes, which goes beyond FR-023's
  "include paths and the doubles' base class". **Recommendation**: move the table into the renderer's
  `getAttribute`, remove the 28, and delete the overrides; amend FR-023 to allow exactly that. The
  alternative is to keep them as dead members until a later regroup.

  **Decided 2026-10-30**: accepted. The table moves into `getAttribute`, the 28 methods are removed and their overrides deleted; FR-014 and FR-023 are amended.

- **Q4. Type mismatch.** *(Superseded 2026-10-31 by F1: see "Tasks-stage decisions" and FR-026; the text below is the plan-stage record.)* Master never refuses on type: a larger stored value overflows a smaller
  destination. **Recommendation**:
  - the service reports `TypeMismatch`;
  - Shading treats it exactly as `Found`, so the output is byte-identical;
  - the contract pins "`TypeMismatch` never changes a result in 2.0.0", and the overflow is filed as
    an issue for a later, output-changing fix.

  **Decided 2026-10-30**: accepted. `TypeMismatch` is reported and treated as `Found`.

- **Q5. Move the `.slo` loader into Shading.** `parseSloShader()` (`rendererFiles.cpp:53-343`, no
  `CRenderer::` reference) is the only way to load a `.slo`, and it is in the core. SC-001's JIT half
  needs it without `ribRender`. **Recommendation**:
  - move it verbatim into `libshader/shading/sloLoader.cpp`; the core keeps calling it; no behaviour
    change, no new edge;
  - it is a first piece of spec 033's "loading in Shading", and DB-007 is unchanged;
  - the alternative is that the JIT half links `ribRender` for loading only, and SC-001 is amended.

  **Decided 2026-10-30**: accepted. `parseSloShader()` moves verbatim into `sloLoader.{h,cpp}`.

- **Q6. What the standalone program can test, and the irradiance rays.**
  - The bundles' `postShade` bodies and the irradiance cache are Hiders code. A standalone tracer
    double can record `trace`/`transmission`/`gather` requests, but cannot make those builtins consume
    canned answers.
  - `occlusion()`/`indirectdiffuse()` rays are cast by Hiders (`irradianceDispatch.cpp:371, 471`),
    not by Shading.

  **Recommendation**:
  - the standalone program covers every request plus canned answers for the single-ray probes;
  - a `ribRender`-linked program covers bundle and cache consumption with a canned tracer;
  - route the two `irradianceDispatch.cpp` calls through `libshader::traceRay`, so FR-011's
    `occlusion`/`indirectdiffuse` rays reach the tracer.

  Compiling `bundles.cpp` into the standalone test was rejected: a Hiders object would be on its link
  line (US1 AS1).

  Build membership was checked. `irradianceDispatch.cpp` is compiled only into `ribRender`, which
  links `libshader_shading`. `ribVector` removes it and uses `vector/irradianceDispatchStub.cpp`
  instead (`src/ri/CMakeLists.txt:359-397`). The new references to `libshader::traceRay` and the
  observer therefore link everywhere the file is built. `hiderTracer.cpp` joins the same
  `REMOVE_ITEM` list, because it calls `trace()`/`traceEx()`, which `ribVector` does not build.

  **Decided 2026-10-30**: accepted. Two programs, and `irradianceDispatch.cpp`'s two trace calls go through `libshader::traceRay`.

- **Q7. SC-005's recording point.** Any observer that exists on the starting tree is production code.
  **Recommendation**:
  - a static `CShadingContext::traceRequestObserver`, null in production, called at the 10 + 2 call
    sites in Phase 1 and moved into the `traceCalls` helpers in Phase 8 (spec 039's `pointKeyObserver`
    is the precedent);
  - a single-threaded replay per platform;
  - characterization wording as spec 028's Q3 (green by construction, red by mutation); amend SC-005
    and FR-023 to match.

  **Decided 2026-10-30**: accepted. A null-in-production `traceRequestObserver`, with characterization wording; SC-005 and FR-023 are amended.

- **Q8. `shadername(type)`** reads the bound shaders from the object's attributes (`shading.cpp:2808`).
  It is not one of FR-006's six. **Recommendation**: leave it, list it among SC-002's remaining reads
  and in DB-018's "still open", and have specs 033/034 route it. The alternative is a seventh
  `getMessage` use with no parameter name.

  **Decided 2026-10-30**: accepted. `shadername(type)` stays a direct read.

- **Q9. One version constant** for the services, `ITracer` and `ShaderGlobals` (D22's "renderer-services
  API"). **Recommendation**: one, `kRendererServicesVersion{2, 0, 0}`. The alternative is three
  independent constants.

  **Decided 2026-10-30**: accepted. One constant, `kRendererServicesVersion{2, 0, 0}`.

- **Q10. `RayType` granularity.** **Recommendation**: `None`, `Camera`, `Secondary`, `Shadow`,
  `Photon`, appending later. The code has one hit mode for every secondary ray (`specularhitmode`), so
  a producer could not fill a finer split today. The alternative is OSL's split now (reflection,
  refraction, diffuse, glossy).

  **Decided 2026-10-30**: accepted. 5 `RayType` values.

- **Q11. `setOffendingObject(CObject *)`, and the one Hiders type left in the interface.**
  **Recommendation**:
  - replace the parameter with `ObjectHandle`; that removes the Geometry type, and the upcast is what
    needs `surface.h`;
  - read FR-003 as applying to the v2 members, and record `getPhotonMap()`'s `CPhotonMap *` (Hiders)
    as the exception FR-013 keeps for spec 032. `CTexture3d` is Texturing, which is allowed.

  **Decided 2026-10-30**: accepted. `setOffendingObject(ObjectHandle)`; `CPhotonMap *` is the one exception.

- **Q12. A null object.** On master, `attribute()`, `surface()` and the other object queries
  dereference a null `currentObject` (the imager, `imager.cpp:82`). **Recommendation**: the renderer
  answers `NotFound` for a null `ObjectHandle`. Only a path that crashes on master changes; no image
  changes. Record it and file it. The alternative is an `assert`.

  **Decided 2026-10-30**: accepted. `NotFound` for a null object.

- **Q13. Spec text that the measurements contradict.** **Recommendation**: amend the following, with
  no change in intent:
  - "56 methods" -> 57;
  - the Assumptions' Linux line (Maestri and the maintainer's rsync) -> "the agent over ssh in the
    per-spec tree `~/Projects/Development/CLI/openrender-026`, remote `cpujob -j2`; the Linux baseline
    is a gate before Phase 2";
  - plus Q1's, Q2's, Q3's and Q7's wording.

  **Decided 2026-10-30**: accepted. 57 methods and the ssh Linux line are in the spec.

- **Q14. `getMatrix`'s time.** None of the call sites carries one. **Recommendation**: Shading passes
  0, the normalised shutter-open time of `CRay::time`, documented. A double may honour the argument,
  and the renderer ignores it (FR-008).

  **Decided 2026-10-30**: accepted. `getMatrix` receives time = 0.

## Tasks-stage decisions (2026-10-31)

The maintainer accepted the recommended answer to QA to QG and set the triage rule for findings. Each is recorded in spec.md, Clarifications,
"Session 2026-10-31". The plan above is amended to match: the wrapper forwarding lines (QA), the four `NoServices` tests (QB), the 6-symbol
stub until Phase 8 (QC), the double's TEMPORARY block (QD), the null object in the standalone program (QE), a TEST-APPROVAL in every phase (QF),
`objectHandle()` in Phase 3 (QG), and the findings F1 to F4 resolved in Phases 5 and 6 with the tests of T-F1 to T-F4. The out-of-scope
findings O1 to O7 are filed as #115 to #121 (`validation.md` §4.3 keeps the text; evidence is added as a comment).

The analyze stage (2026-10-31) found 4 HIGH, 11 MEDIUM and 8 LOW issues and the maintainer approved every remediation. The ones that change this plan:
the forwarder's TEMPORARY block (A2); the expected type accepted by all eight parameter members (A3); a rendered `LightsourceNoLoopReal` (A4); a sweep image
manifest of the starting tree on each platform, diffed in Phase 10 (B1); the `OPENRENDER_HAVE_LLVM` guard moving with `sloLoader`, and a Phase 3 imports-subset
assertion (B2); the JIT's crossed accessors in F4 (B3); no `TypeMismatch` check for source `Light` (B4); the Phase 3 JIT-off compile (B9); the direct-call
tests in executables of their own (B10); rows 6 and 7 of the situation table asserted from `classifyShading()` alone (B8). FR-010's concurrency answer is
argued in `contracts/tracer.md`, not tested (accepted, C6).

## Reference-document decisions (D1-D30, §5.5, §8.1, §8.3, §11.1)

- **D22**: one renderer-services version, independent of the RSL language and closure ABI versions (Q9).
- **D25**: the boundary stays internal C++; the public C facade is spec 052's.
- **D20, §8.1**: the footprint is √ShadingRate pixels, taken from the stored value and never
  square-rooted again (R12).
- **§5.5, §8.3**: `ShaderGlobals` carries both grid differences and ray differentials, with explicit
  absence rules. Derivatives are optional and standardized (R11).
- **§11.1 step 1**: the four services and `ITracer` are the surface spec 049's OSL `RendererServices`
  adapter maps onto, with the names `get_matrix`, `get_attribute`, `getmessage` and `trace` written in
  this tree's camel case.
- **D30**: everything new lives in Shading or is implemented by the core and Hiders; no new domain.
- No decision is changed.

## Complexity Tracking

None. The Constitution Check has no violation to justify.
