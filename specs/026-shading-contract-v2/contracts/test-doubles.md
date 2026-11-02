# Contract: test doubles, the load-time stub and the test programs

**Decisions**: every plan question referenced here (Q1-Q14) was decided on 2026-10-30. The maintainer accepted each recommended answer; they are recorded in spec.md, Clarifications, "Session 2026-10-30 (plan-stage decisions)". Where this file says "(Qn)", the design described is the decided one.

**Requirements**: FR-004, FR-023, FR-024, FR-026 to FR-028; SC-001, SC-003 to SC-006, SC-012 to SC-014.
**Directory**: `tests/shading_contract/` (new; `tests/` is not a domain for archcheck). Label
`shading_contract`; the replay recorder carries `shading_contract-record`.

## `CContractServices`: the standalone services double

A complete `CRendererServices` v2 with no inner implementation (FR-004). It records every call in
order: method, request fields, answer.

| Method group | Answers from |
|---|---|
| grid/globals | `maxGridSize` 64; the 29 built-in globals built with `parseVariable` from `initDeclarations()`'s declarations (`ShadingContract_DoubleGlobalsMatchRenderer` checks them) |
| timing, raster, clip, bounds, `additionalParameters`, `hasIlluminationHook`, `shootStep` | fields the test sets |
| `getAttribute` | a table `(scope, object handle, name) -> (type, count, values)`; anything else `NotFound`; a per-test switch answers `Unavailable` |
| `getMessage` | a table `(source, object handle, light handle, name) -> default value, or (variable, globalIndex)`; the entry's `variable` is returned only when the request has `wantVariable`, and then nothing is written into `dest` (the header's "dest is written XOR a variable is returned") |
| `getMatrix` | a table `name -> (kind, from, to)`, with time honoured when the test sets per-time entries; the name `"object"` with a null handle -> `NoObject` (the double special-cases the name `"object"`, which is not in its table, where the header and the renderer decide by the kind `COORDINATE_OBJECT`; the two agree for that name, and a proposed edit makes the double decide by kind as well) |
| `tracer()` | the test's `CRecordingTracer` or `nullptr` |
| GI/3D getters, `textureSystem`, lookups, `getDSO`, `getGlobalID` | `nullptr` or 0, recorded |
| `setOffendingObject` | recorded |

It carries `static_assert(kRendererServicesVersion.major == 2)`. SC-012's review compares the header
documentation with this double. The reviewer is someone who did not write it.

- **Temporary block (QD, 2026-10-31).** The methods that stay pure virtual until their phase removes them (the two coordinate-system
  lookups, Phase 4; the 25 option getters and `lookupUserOption`, Phase 5; `setOffendingObject(CObject *)`, Phase 7) are implemented in one
  delimited block marked `TEMPORARY (spec 026)`, each recording its call. Without it the double is abstract in Phase 3. Each removing phase
  deletes its part, named at that phase's TEST-APPROVAL.
- **No object in the standalone program (QE).** `CSurface` is Geometry, so the standalone program runs with a null current object and the
  double is keyed on the null `ObjectHandle`. Handle identity (a real `CProbeSurface`) is checked in the rendered program.

The four existing whole-interface classes (`CForwardingServices`, `CRecordingServices`,
`CStubServices`, `CTextureServices`) are kept (FR-004). Their edits are: deleting the overrides of
the 28 removed methods and changing `setOffendingObject`'s parameter type (Q3); and, in the three that forward to an inner
implementation (`CForwardingServices`, `CRecordingServices`, `CTextureServices`), one forwarding line each for `getMatrix`, `getAttribute`,
`getMessage` and `tracer()`, added in the phase that makes the member live (QA, 2026-10-31). `CStubServices` answers fixed sentinels and gets none.

`CContractForwarder` (`contract_forwarder.h`, new, rendered program only) is a complete `CRendererServices` that forwards every member to the
services installed before it and records the v2 ones. Like `CContractServices` it forwards the methods that stay pure virtual until their phase in a
delimited `TEMPORARY (spec 026)` block, deleted in Phases 4, 5 and 7 and named at those phases' TEST-APPROVALs; without it the forwarder stops compiling when a
member is removed (A2). `RealAnswers*` uses it: `CRendererServicesImpl` is `final`, and a new class keeps
those tests independent of the wrappers.

## `load_stub.cpp`: the load-time stub (FR-024)

It exists only so that `libshader_shading` loads in a process without `libRIBRender`. Every body
traps:

```cpp
[[noreturn]] static void stubTrap(const char *what) {
    fprintf(stderr, "load_stub: %s reached; this path needs ribRender (spec 026, FR-024)\n", what);
    abort();
}
```

| Symbol (Itanium) | Demangled | Defined by |
|---|---|---|
| `_ZTV12CTraceBundle` | vtable for `CTraceBundle` | `CTraceBundle::postTraceAction()` (key function), `postShade(int, CRay **, float **)`, `postShade(int, CRay **)`, `post()` |
| `_ZTV19CTransmissionBundle` | vtable for `CTransmissionBundle` | the same four members |
| `_ZN13CGatherBundleC1Ev` | `CGatherBundle::CGatherBundle()` | the constructor, `~CGatherBundle()` and the four virtuals (the constructor needs the class's vtable) |
| `_ZN10CPhotonMap6lookupEPfPKfi` | `CPhotonMap::lookup(float *, const float *, int)` | that member |
| `_ZN15CShadingContext5traceEP4CRay` | `CShadingContext::trace(CRay *)` | **TEMPORARY (QC):** that member; deleted in Phase 8 |
| `_ZN15CShadingContext7traceExEP10CRayBundle` | `CShadingContext::traceEx(CRayBundle *)` | **TEMPORARY (QC):** that member; deleted in Phase 8 |

The table is 6 symbols while the starting tree still imports `trace`/`traceEx` (Phases 2 to 7), and 4 after Phase 8.

- **Includes**: `ri/hiders/bundles.h` and `ri/hiders/photonMap.h`. The stub is compiled into the
  standalone executable only.
- **Measured at Phase 1** on both platforms, and recorded in `tools/libshader-imports-{base,after}-{macos,linux}.txt`:
  - macOS: `nm -m` and `dyld_info -fixups`;
  - Linux: `nm -D --undefined-only` and `readelf -d` (`FLAGS`/`BIND_NOW`).
- **What the table must match**: the final table equals the measured after-list. A symbol that is
  imported but missing from the stub fails the program's start, and the test fails with it.
- **Deleted by spec 029**, which removes the imports.

## Test programs

| Program / tests | Links | Red on the starting tree because | Covers |
|---|---|---|---|
| `test_shading_contract_standalone`: `ShadingContract_StandaloneLink`, `_Attributes{Interp,Jit}`, `_Messages{Interp,Jit}`, `_Matrices{Interp,Jit}`, `_TraceRequests{Interp,Jit}`, `_NoServices_{Matrix,Attribute,Message,Tracer}` (four tests, QB), `_ShaderGlobalsDouble`, `_TypeMismatch_{Attribute,Message}{Interp,Jit}`, `_LightsourceNoLoop{Interp,Jit}` | `libshader_shading`, `openrendercommon`, `state_only_sources`, `load_stub.cpp`, the doubles. **No `ribRender`**. `*Jit` only with `OPENRENDER_HAVE_LLVM` | build failure (`serviceTypes.h`, `tracer.h`, `shaderGlobals.h`, `sloLoader.h` do not exist) | US1, US3 AS1, US4 AS1-AS3, US5 AS1/AS3, FR-023(a)-(d), (f). `_TraceRequests` covers only what does not trap: the probe forms (`visibility`, probe `transmission`, the reflection probe), with canned answers, and the **requests** of `trace()`'s and `transmission()`'s bundles. Constructing those bundles on the stack only writes the vtable pointer, and the recording tracer never runs their protocol. `_NoServices_*` covers the three queries and the no-tracer **probe** forms. `_TypeMismatch_*` puts a canary beside the destination (FR-026; source `Light` is exempt, B4); `_LightsourceNoLoop` pins FR-028 (the interpreter crashes on master, the JIT already guards) |
| `ShadingContract_StandaloneLinkLine` (a CMake script test) | | the program does not exist | US1 AS1: the link command has no `libRIBRender`, `ribRender_static` or any `src/ri/{render,hiders,geometry}` object |
| `test_shading_contract_rendered`: `ShadingContract_RealAnswers{Interp,Jit}`, `_GatherRequests{Interp,Jit}`, `_CannedTrace{Interp,Jit}`, `_NoTracer`, `_DoubleGlobalsMatchRenderer`, `_NullObject_{Attributes,Messages}{Interp,Jit}`, `_LightsourceNoLoopReal{Interp,Jit}` | `ribRender`, `libshader_shading`, `openrendercommon` | build failure | US3 AS2, US6 AS1-AS3, SC-003 (real side), SC-006, FR-023(e). Here and not in the standalone program: `gather()`, which runs `new CGatherBundle` (`shading.cpp:4566`) and would trap on the stub's constructor; the no-tracer **bundle** forms, whose miss path runs `bundle->postShade()`; every canned answer consumed through a bundle; `occlusion`/`indirectdiffuse` (Hiders' irradiance cache) |
| `ShadingContract_TraceReplay_<builtin>` (trace, transmission, visibility, gather, environment, shadow, occlusion, indirectdiffuse) | `ribRender` | **characterization**: green by construction on the starting tree; shown red by a per-builtin mutation (Q7) | SC-005 |
| `test_shading_contract_typemismatch_attr` (Phase 5), `test_shading_contract_typemismatch_msg` (Phase 6): `ShadingContract_TypeMismatchReal_{Attribute,Message}{Interp,Jit}`; `test_shading_contract_offending` (Phase 7): `ShadingContract_OffendingObject` | as the rendered program | red until their phase: the attribute family calls `attributes()`/`options()`/`rendererInfo()` directly, whose signatures change in Phase 5 (build failure, B10); the message family calls `surfaceParameter()` and the other four, which gain `QueryType` in Phase 5 as well, so it compiles from Phase 5 and its red evidence at the Phase 6 gate is the overwritten canary (the members ignore `expected` until Phase 6, N1); `OffendingObject` asserts the `setOffendingObject(ObjectHandle)` the double receives (build failure, or the direct-helper fallback, N6) | FR-026, SC-013, FR-003 (Q11) |
| `test_shading_contract_situations`: `ShadingContract_Situations` | as the rendered program | build failure until Phase 9: it reads report fields that Phase 9 adds, so it has an executable of its own | US5 AS2, FR-017, FR-018, SC-006 |
| `ShadingContract_TraceReplayRecord` (label `shading_contract-record`) | `ribRender` | | writes a fresh replay into the build tree only |

Further details:

- **The JIT and the interpreter.** Each `*Jit` test runs the same shader compiled with `oshader --jit`
  (a build step, as `shaders/CMakeLists.txt` does), and the `*Interp` twin runs the `.rslo`. The
  pair's recorded requests must be equal, except for `lightsource` (research R7; the test asserts the
  documented difference instead).
- **No existing test is edited** beyond the four doubles' overrides (Q3), the three wrappers' forwarding lines (QA), plus the trailing fields
  of the two spec-039 observer reports, which existing readers ignore.
