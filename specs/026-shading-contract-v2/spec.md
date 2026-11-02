# Feature Specification: Shading Contract v2

**Feature Branch**: `026-shading-contract-v2`
**Created**: 2026-10-30
**Status**: Draft
**Input**: Constitution Principle VIII remediation, layer L3, after specs 024 and 025 (both merged), GitHub #32. Give Shading an OSL-shaped boundary: a second version of the renderer-services interface (`get_matrix`, `get_attribute`, `getmessage`, trace through a Shading-owned `ITracer`, texture through the existing texture system of spec 027), a `ShaderGlobals` description of a batch of shading points with an explicit ray type and hit mode, and the existing implementation adapted behind them. The interfaces stay internal C++, carry a version constant from day one, and are covered by test doubles. Resolves the service part of registry entry DB-018 and reduces DB-006 (the contract).

## Clarifications

### Session 2026-10-30

- Q: Which service carries which builtin (FR-006)? -> A: `get_attribute` carries `attribute()`, `option()` and `rendererinfo()`; `getmessage` carries the six shader-to-shader queries (`surface`, `displacement`, `atmosphere`, `incident`, `opposite`, `lightsource`). (maintainer)
- Q: How far does the trace service go (FR-010, FR-024)? -> A: Define `ITracer`, route Shading's call sites through it, adapt the hiders' code behind it, claim no DB-001 or DB-005 allowlist line, and supply the residual load-time symbols that spec 029 deletes from a stub in the test directory. (maintainer)
- Q: Is the tracer's vocabulary today's bundles or OSL-shaped single rays (FR-010)? -> A: Today's `CRay`/`CRayBundle` vocabulary; the OSL-shaped single-ray adapter is spec 049's. (maintainer)
- Q: `get_matrix` time? -> A: Carried in the signature only; the renderer's implementation returns the same matrix at every time, and a test double may honour it. (maintainer)
- Q: `ShaderGlobals` layout? -> A: Structure-of-arrays with a runtime point count; per-point fields are a pointer or span plus a stride; a batch of one is valid; no fixed batch width. (maintainer)
- Q: When is `ITracer` installed? -> A: Per frame through the services, registered in `beginFrame()` and cleared in `endFrame()`; the calling context is passed as a parameter. The plan must confirm that hiders running concurrently in one frame (photon pass versus camera pass) cannot overwrite each other. (maintainer)
- Q: Are the 56 existing `CRendererServices` methods regrouped? (The plan measured 57; see the plan-stage session below.) -> A: No. The v2 services are added as new members; the plan classifies each existing method as unchanged, replaced or removed (FR-014). (maintainer)
- Q: What does a failed `get_attribute`/`getmessage` query return? -> A: An explicit status (found, not found, type mismatch); the conversion and refusal logic stays in Shading, applied to the service's raw typed value. (maintainer)

### Session 2026-10-30 (plan-stage decisions)

The maintainer accepted the plan's recommended answer to each of its 14 questions (plan.md, "Open questions"). The spec text below is amended to match.

- Q1: Routing Shading's trace calls through `ITracer` leaves three DB-001 link lines stale. Do they go? -> A: Yes. Delete `allowlist.txt`'s `executeMisc.cpp -> trace(CRay*)`, `executeMisc.cpp -> traceEx` and `shading.cpp -> traceEx`. DB-001 goes from 8 to 5 lines and from 6 to 4 imports, and stays Open. The bundles and the photon-map lookup stay for spec 029. FR-010, FR-020 and the Assumptions are amended. (maintainer, plan-stage recommendation accepted)
- Q2: `lightsource()` differs between the backends: the interpreter reads the iterated light, while the JIT's `op_lightsource_f` reads the last light that ran, takes floats only and writes the value into the result. -> A: Both go through `getMessage` with source `Light`, and each keeps its own light, variable request and result handling, so each backend's output is unchanged. The edits are a new member used by the macro and `op_lightsource_f`'s body; there is no emitter or ABI change. The divergence is recorded and filed as an issue. FR-007 and SC-003 are amended. (maintainer, plan-stage recommendation accepted) **Reaffirmed 2026-10-31 (O1):** the divergence is out of scope and filed; the null-light crash of the interpreter's `lightsource()` outside a loop is in scope (F3, FR-028).
- Q3: Where does the option name table live, and are the methods it replaces removed? -> A: The table moves into the renderer's `getAttribute`. The 28 methods left without a caller are removed: `findCoordinateSystem`, `findCoordinateSystemWithType`, the 25 option getters and `lookupUserOption`. Their overrides are deleted from the four whole-interface test doubles. FR-014 and FR-023 are amended. (maintainer, plan-stage recommendation accepted)
- Q4: Master never refuses on type. What does `TypeMismatch` do? -> A: The service reports it, and Shading treats it exactly as `Found`: it copies the stored layout and returns found = 1. In 2.0.0 the status never changes a result. The overflow of a smaller destination is filed as an issue. (maintainer, plan-stage recommendation accepted) **Superseded 2026-10-31 by F1 (Session 2026-10-31 below):** a mismatch that cannot be copied safely is refused (FR-026); the residual overflow of a float-class destination of unknown size is filed (O7).
- Q5: How does the standalone program load `.slo` shaders? -> A: `parseSloShader()` and its helpers (`rendererFiles.cpp:53-343`, with no `CRenderer::` reference) move verbatim into `src/libshader/shading/sloLoader.{h,cpp}`. The core keeps calling it. (maintainer, plan-stage recommendation accepted)
- Q6: What does the standalone program test, and how do the rays of `occlusion`/`indirectdiffuse` reach the tracer? -> A: There are two programs. The standalone one, with no `ribRender`, covers the queries, the probe rays and the requests of `trace()`/`transmission()`. A `ribRender`-linked one covers `gather()`, every answer consumed through a bundle, the no-tracer bundle forms and the irradiance cache. `irradianceDispatch.cpp`'s two trace calls go through `libshader::traceRay`; that file is built only into `ribRender`, which links `libshader_shading`. (maintainer, plan-stage recommendation accepted)
- Q7: Where is SC-005's replay recorded? -> A: At a null-in-production observer, `CShadingContext::traceRequestObserver`. Phase 1 places it at the 10 Shading call sites and the 2 irradiance call sites; Phase 8 moves it into the `traceCalls` helpers. The replay runs single-threaded, one file per platform. The replay tests are characterization tests: green by construction on the starting tree, shown red by mutation. SC-005 and FR-023 are amended. (maintainer, plan-stage recommendation accepted)
- Q8: Is `shadername(type)` routed through `getMessage`? -> A: No. It stays a direct read, and is listed among SC-002's remaining reads and in DB-018's "still open". (maintainer, plan-stage recommendation accepted) **Amended 2026-10-31 by F2:** it stays a direct read but is null-guarded (FR-027).
- Q9: One version constant or several? -> A: One, `kRendererServicesVersion{2, 0, 0}`, for the services, `ITracer` and `ShaderGlobals` (D22). (maintainer, plan-stage recommendation accepted)
- Q10: How fine is `RayType`? -> A: Five values, `None`, `Camera`, `Secondary`, `Shadow` and `Photon`, appended to later. (maintainer, plan-stage recommendation accepted)
- Q11: What about `setOffendingObject(CObject *)` and the types left in the interface's signatures? -> A: It becomes `setOffendingObject(ObjectHandle)`. FR-003 applies to the v2 members. `getPhotonMap()`'s `CPhotonMap *` is the one Hiders type left, kept by FR-013 for spec 032. (maintainer, plan-stage recommendation accepted)
- Q12: What does a query with no current object return? -> A: `NotFound` for a null `ObjectHandle`. Master crashes on that path; no image changes. It is recorded and filed. (maintainer, plan-stage recommendation accepted) **Extended 2026-10-31 by F2:** FR-027 requires it, and extends it to `shadername(type)`.
- Q13: Spec text the measurements contradict? -> A: The interface has 57 methods, not 56. Linux is validated by the agent over ssh in the per-spec tree `~/Projects/Development/CLI/openrender-026`, with the remote `cpujob` at `-j2`, and the Linux baseline is a gate before Phase 2. (maintainer, plan-stage recommendation accepted)
- Q14: Which time does `getMatrix` receive? -> A: 0, the normalised shutter-open time of `CRay::time`. The renderer ignores it; a double may honour it. (maintainer, plan-stage recommendation accepted)

### Session 2026-10-31 (tasks-stage decisions and finding triage)

The tasks stage raised seven questions (QA to QG) and triaged the findings of the plan stage. The maintainer accepted the recommended answer to each of QA to QG and set the triage rule: **a finding whose context matches or impacts this spec is resolved in spec 026; anything else is filed on GitHub as P1** (the existing label `priority: high`). The full classification and the text of the issues are in `validation.md` §4. The spec text above and below is amended to match.

- QA: May the three forwarding wrappers (`CForwardingServices`, `CRecordingServices`, `CTextureServices`) gain forwarding overrides of the new members? -> A: Yes. Each gains a one-line forwarding override of `getMatrix`, `getAttribute`, `getMessage` and `tracer()`, added in the phase that makes the member live and named at that phase's TEST-APPROVAL. `CStubServices` answers fixed sentinels, has no inner implementation, and gets none. FR-023 is amended. Without them an existing test that installs a wrapper would silently lose coordinate systems, options or traces. (maintainer; **Decided 2026-10-31**)
- QB: The single `NoServices` test cannot be green in Phase 3, because no service is routed yet. -> A: It is four tests, `NoServices_Matrix`, `_Attribute`, `_Message` and `_Tracer`, each enabled in the phase that routes its service. (maintainer; **Decided 2026-10-31**)
- QC: The load stub needs `CShadingContext::trace` and `traceEx` as well while the starting tree still imports them. -> A: The stub carries 6 symbols until the tracer phase and 4 afterwards; the two extra traps are deleted in Phase 8, with that phase's approval. (maintainer; **Decided 2026-10-31**)
- QD: The services double is abstract in Phase 3 unless it implements the methods that stay pure virtual until Phases 4, 5 and 7. -> A: It carries a delimited TEMPORARY block of recording overrides, deleted by the phase that removes each method and approved at that phase's TEST-APPROVAL. (maintainer; **Decided 2026-10-31**)
- QE: The standalone program cannot construct a `CSurface` (Geometry). -> A: It runs with a null current object and a double keyed on the null `ObjectHandle`; handle identity is checked in the rendered program with a real `CProbeSurface`. (maintainer; **Decided 2026-10-31**)
- QF: The plan writes every new test in Phase 2, while the maintainer's rule is a TEST-APPROVAL in every phase. -> A: Phase 2 keeps the bulk approval. Every later phase starts with a Test gate (its tests enabled and shown red) and ends its Tests block with its own TEST-APPROVAL, which names any edit to an existing test; a phase that changes no test gets a one-word confirmation. Tests for F1 to F3 are written in the phase that fixes them. (maintainer; **Decided 2026-10-31**)
- QG: `objectHandle()` is needed from Phase 4. -> A: It and the shader-handle helper are added in Phase 3 (no caller, no behaviour change); Phase 7 adds only `objectAttributes()` and the `setOffendingObject` change. (maintainer; **Decided 2026-10-31**)

Decisions of the analyze stage (2026-10-31): the analysis of spec, plan and tasks found 4 HIGH, 11 MEDIUM and 8 LOW issues; the maintainer approved every recommended remediation. Four of them change the spec:

- B1: SC-007 stays as written (byte-identical images for the determinism sweep too). Phase 1 therefore records a sweep image manifest on the starting tree, on each platform, and the final phase diffs against it. (maintainer; **Decided 2026-10-31**)
- B3: F4/FR-029 covers the crossed accessor constants in the JIT as well (`jitIncidentParameter` passes `ACCESSOR_EXTERIOR`, `jitOppositeParameter` `ACCESSOR_INTERIOR`, `shading.cpp`); the `MessageAccessors` test covers both, and F4 is folded into SC-013. (maintainer; **Decided 2026-10-31**)
- B4: the `TypeMismatch` check is not applied to source `Light`. `lightsource()` keeps each backend's output (Q2, FR-022), and the interpreter and JIT disagree about the destination there. FR-026 is amended. (maintainer; **Decided 2026-10-31**)
- B8: situation-table rows 6 (implicit-surface hits) and 7 (DSO-object hits, O2 / #116) are asserted from `classifyShading()` alone, like rows 10 to 14, because their present behaviour is an oddity and not a case to render. FR-018 is amended. (maintainer; **Decided 2026-10-31**) **Update 2026-11-01**: #116 (`CDLObject::shade` passed `vVertices = -1`) was fixed by #129 and closed; the DLO vertex count is now 1. Row 7 stays classify-only as decided (kept as decided; it could be rendered later now that the vertex count is fixed); its reason is no longer an oddity.

Triage, in scope (resolved in this spec, tests first):

- F1 (R16.1): the copy of an attribute, option, renderer-info or message-passing value is type-checked as far as it can be without the destination's length. A `TypeMismatch` is refused: destination untouched, 0 returned, one warning. Supersedes Q4. FR-026, SC-013.
- F2 (R16.5, Q12): a query with no current object answers not found for the six object builtins (`attribute`, `surface`, `displacement`, `atmosphere`, `incident`, `opposite`) and `shadername(type)`; `option()` and `rendererinfo()` do not need an object and answer as usual; nothing dereferences a null object. FR-027, SC-014.
- F3 (R7): the interpreter's `lightsource()` outside an `illuminance`/`illuminate` loop answers not found instead of dereferencing a null light. FR-028, SC-014.
- F4 (R7): the accessor constants of `incident()`/`opposite()` agree with the source they ask for, in the interpreter's `DEFFUNC` lines and in the JIT's `jitIncidentParameter`/`jitOppositeParameter` (`shading.cpp`); no output changes. FR-029, SC-013.

Triage, out of scope (filed as P1: O1 #115, O2 #116 (resolved by #129, closed 2026-11-01), O3 #117, O4 #118, O5 #119, O6 #120, O7 #121; evidence is added as a comment on the issue; the text of each is kept in `validation.md` §4.3):

- O1: the JIT/interpreter `lightsource()` divergence (Q2 keeps each backend's output; spec 033 owns the JIT entry points).
- O2: `CDLObject::shade` passed `vVertices = -1` (a Geometry source, spec 031; memory-safety). **Resolved by #129 (closed 2026-11-01)**: `dlobject.cpp` now passes 1; kept here as the record.
- O3: `camerahitmode` honoured only for REYES grids (situation oddities 1, 2, 8).
- O4: `diffusehitmode` parsed and never read (oddity 5).
- O5: implicit-surface ray hits shaded 0-D (oddity 3).
- O6: three behaviours to verify against the RISpec (oddities 6 and 7, and the `incident`/`opposite` side).
- O7: the destination size of a float-class query is unknown to the builtin, the residual of F1.

## User Scenarios & Testing *(mandatory)*

The "users" of this feature are the people who build, maintain and extend openRender: contributors working in the Shading domain, maintainers reviewing remediation specs, the authors of the specs that build on this boundary (029 hider/shading separation, 033 shading-system backends, 034 shader networks, 049 the OSL backend, 052 the public runtime library), and anyone rendering scenes that use message passing, attribute and option queries, coordinate-system transforms and traced lookups.

### Measured starting point (2026-10-30, master `d01da47`)

Method: static counts from the source and the allowlist of this worktree (no build was run for the specify stage). The plan re-measures them with `archcheck` on a fresh macOS build, and the first implementation tasks re-take them once more after any rebase (spec 027's practice).

- **The services interface is one growing class with one real implementer.** `CRendererServices` (`src/libshader/shading/rendererServices.h`) has 57 pure virtual methods, plus the virtual destructor (corrected at the plan stage from 56; two declarations span two lines): frame and option getters, resource getters, coordinate-system lookup, filter/DSO/variable lookup, the 3D-texture channel forwarders (spec 027) and `textureSystem()` (spec 027). `CRendererServicesImpl` (renderer core) is its only production implementer; four test classes implement the whole interface (`CRecordingServices`, `CForwardingServices`, `CStubServices`, `CTextureServices`; plan-stage count). Those that forward do so to a real inner implementation, and none is a standalone double. It carries no version.
- **Message passing and attribute reads bypass the services.** `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()`, `attribute()` and the shader-name query read `currentShadingState->currentObject->attributes` themselves (`shading.cpp`, the `*Parameter` members and `attributes()`), resolving the bound shader through spec 025's handle accessors. `lightsource()` reads the current light entry. `option()` already goes through the services (`lookupUserOption` and the option getters). Further reads of the same field sit in `execute.cpp` (2 sites), `executeMisc.cpp` (2 sites), `shaderFunctions.h` and `giFunctions.h` (flags, shading rate, photon estimator). The JIT's `op_*_param` entry points call the **same** `CShadingContext` members as the interpreter, so a change inside those members reaches both backends.
- **Coordinate systems are half routed.** Named systems come from `findCoordinateSystemWithType` on the services; the object, shader and light systems are resolved inside Shading from `currentObject->xform`, `currentShaderInstance->xform` and `currentLightInstance->xform` (`shading.cpp:2855-2910`). About 30 call sites reach it (31 mentions in the shading sources, declarations and comments included). No call carries a time.
- **Tracing is a member of the shading context whose body lives in Hiders.** `trace(CRay *)` and `traceEx(CRayBundle *)` are declared in `shading.h` and defined in `src/ri/hiders/trace.cpp`; `executeMisc.cpp` (transmission, visibility, trace) and `shading.cpp` (gather) create the hiders' bundle classes (`CTraceBundle`, `CTransmissionBundle`, `CGatherBundle`, declared in `src/ri/hiders/bundles.h`) and call them. These account for 6 of DB-001's 8 allowlisted link edges (`trace`, `traceEx` twice, the two bundle vtables, `CGatherBundle`'s constructor; the other 2 are `CPhotonMap::lookup`); the include side is DB-005's 8 edges. The bundle vtables are **data** imports, which dyld binds eagerly, so `libshader_shading` cannot even load without a definer for them.
- **Ray type and hit mode are inferred, not passed.** `shade()` receives a shading dimension (`SHADING_0D`, `SHADING_2D_GRID`, `SHADING_2D`) and the hit mode is the object's `camerahitmode` attribute, read only when the dimension is a grid (`shading.cpp:1374`); the photon pass is told apart by the illumination hook (spec 024); a ray carries `flags`, `depth` and a sample key (spec 039). There is no single value that says "this is a camera hit", "a shadow ray", "a photon" or "a grid".
- **Shading's coupling to Geometry (DB-006) has 3 Shading-origin include edges**: `execute.cpp` and `executeMisc.cpp` to `surface.h`, `shading.cpp` to `object.h`. DB-006's allowlist holds 33 lines in all (26 include, 7 link). `shade(CSurface *, ...)` pulls its inputs from the primitive; switching that to a filled-in structure is spec 029's.
- **Tests that exercise the services link the renderer.** Of the 23 test programs in `tests/shading_parity`, 19 link `ribRender` together with `libshader_shading`. No test runs Shading against a standalone double of the services.
- **Texture access is already routed** (spec 027): `CRendererServices::textureSystem()` and `libshader/shading/textureCalls`; `tests/shading_parity/recording_texture_system.h` is a recording double of the texture system.

### User Story 1 - Shading has one documented, versioned, test-doubled boundary (Priority: P1)

A developer, or a maintainer reviewing a Principle VIII remediation, runs the shading library against a standalone double of its services, with no renderer in the process. Today the services interface is a flat list of 57 methods, only forwarding wrappers of the real implementation exist, and every test that exercises the services links the renderer. After this feature the boundary is a documented v2 interface with a version constant, and a standalone double implements it.

**Why this priority**: It is the remediation this spec exists for, and the stated acceptance of #32. Specs 029, 033, 034, 049 and 052 build on it.

**Independent Test**: Build a test program that links `libshader_shading` and `libRIBCommon` and the double, but no `ribRender`, installs the double as the services, and runs shaders that exercise each v2 service (US3 to US6).

**Acceptance Scenarios**:

1. **Given** the finished tree, **When** the double-based test program is linked, **Then** its link line contains no renderer-core, hider or geometry object, and it runs and passes on macOS and Linux.
2. **Given** the interface headers, **When** a reader opens them, **Then** each service (`get_matrix`, `get_attribute`, `getmessage`, the tracer, the texture system) and the shading-point structure is documented: what it takes, what it returns, what happens when the answer does not exist, what is thread-safe, and which version introduced it.
3. **Given** a service the double does not implement, **When** a shader calls it, **Then** the shading library behaves as it does today with no services (a documented default, one warning per kind), never a crash.

---

### User Story 2 - Renders do not change (Priority: P1)

A user renders any scene, with the interpreter or the JIT, under any hider and thread count. After this feature the images, the diagnostics and the determinism guarantees are the same.

**Why this priority**: The change touches every message-passing, attribute, transform and traced builtin. A pure restructuring that changes a pixel is a regression (as in specs 025, 027 and 028).

**Independent Test**: Run the visual, parity, `shading_parity`, `texture_tile`, determinism and architecture suites on master and on the finished tree; compare pass/fail sets and, on macOS, output bytes.

**Acceptance Scenarios**:

1. **Given** every scene of the visual suite and the determinism sweep, **When** rendered on the finished tree with the same thread count and seed as on master, **Then** on macOS each output file is byte-identical to master's, `.rslo` and `.slo` alike.
2. **Given** the full test suite, **When** it runs on each platform, **Then** its pass/fail set equals master's.
3. **Given** a shader that queries a name that does not exist (an unknown attribute, a shader of a type that is not bound, a missing coordinate system), **When** it runs, **Then** the result and the diagnostic are the same as today. The exceptions are the paths that overflowed a destination, mixed a string with a number, or dereferenced a null object or light on master (FR-026 to FR-028), which refuse or answer not found.

---

### User Story 3 - Message passing and attribute queries go through the services (Priority: P1)

A shader author uses `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()`, `lightsource()`, `attribute()` and `option()`. A developer who embeds the shading library, or who writes the OSL backend, supplies those answers from their own data. Today Shading reaches into the attributes of the current object to answer most of them. After this feature each answer comes from a service call, the same one from both backends.

**Why this priority**: It is DB-018 (V15 step 1), the largest piece of the remediation, and it is what lets Shading stop depending on the layout of Graphics State for per-point data.

**Independent Test**: Install a double whose attribute and message tables are canned; run one shader per builtin under `.rslo` and under `.slo`; compare what the double was asked and what the shader got with the answers the real renderer gives on the same scene.

**Acceptance Scenarios**:

1. **Given** a double with canned answers, **When** an interpreted shader and the same shader compiled for the JIT call each of the builtins above, **Then** the double receives the same request (point, source or object, name, expected type) from both backends and the builtin returns the double's answer.
2. **Given** the real renderer, **When** the same shaders run on the finished tree, **Then** every returned value, including the found/not-found result and the type-mismatch behaviour, equals master's.
3. **Given** the shading sources, **When** they are inspected, **Then** none of the builtins named in this story reads a field of the current object's attributes; the reads that remain (for example the per-object flags the executor tests) are listed in the plan with the reason each stays.
4. **Given** a stored value that cannot be copied safely into the expected type (a matrix asked for as a vector, a string asked for as a float, a number asked for as a string), **When** a builtin of this story asks for it, **Then** it returns 0, leaves the destination untouched (a canary beside the destination is intact) and warns once, under both backends (FR-026).
5. **Given** a shader that runs with no current object (the imager), **When** it calls `attribute()`, `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()` or `shadername(type)`, **Then** the call answers not found (the empty string for `shadername`) and the process does not crash; `option()` and `rendererinfo()` answer as usual (FR-027).
6. **Given** `lightsource()` called outside an `illuminance` or `illuminate` loop, **When** it runs under either backend, **Then** it answers not found and does not crash (FR-028).

---

### User Story 4 - Coordinate systems come from one service (Priority: P2)

A shader author transforms points, vectors and matrices between named spaces, including `"object"`, `"shader"` and `"light"`. A developer substituting the services answers all of them from one `get_matrix`. Today the named, current and world systems come from the services and the object, shader and light systems are computed inside Shading from Geometry and instance objects.

**Why this priority**: It is part of the stated scope and removes one of the reasons Shading holds a primitive. It is P2 because the observable behaviour must not change and the other stories do not depend on it.

**Independent Test**: With a double that answers `get_matrix` from a table, run the transform builtins (`transform`, `vtransform`, `ntransform`, `ptransform`, matrix forms, `texture3d`'s coordinate system) in both directions for each system kind under both backends and compare with master.

**Acceptance Scenarios**:

1. **Given** a double that records `get_matrix` requests, **When** a shader transforms between `"current"`, `"world"`, `"camera"`, a user-declared system, `"object"`, `"shader"` and `"light"`, **Then** every matrix comes from the double (the request carries the shading point's object, the shader instance and the light instance as opaque handles) and the transformed values equal master's.
2. **Given** the request signature, **When** a caller supplies a time, **Then** the service receives it; the renderer's implementation returns the same matrix at every time, as master does (FR-008).
3. **Given** an `"object"` reference when there is no current object, **When** it is evaluated, **Then** the same error text is reported once and the identity matrix is used, as today.

---

### User Story 5 - A shading point has a contract, with a ray type and hit mode (Priority: P2)

A spec author (029, 031, 049, 054) fills in or consumes what Shading needs to shade points. Today that is spread across `CShadingState`, the object, the ray and a few attributes, and the situation is inferred. After this feature `ShaderGlobals` describes a batch (a grid, a span of ray hits, or a single point) with its geometry, derivatives, time, sample identity, path depth, transport mode and shading-cell footprint, plus an explicit ray type and hit mode. Nothing in the render path consumes it yet.

**Why this priority**: V3 step 1 and V9 step 2. It is the contract 029 switches `shade()` to and 049 adapts to OSL's `ShaderGlobals`; getting its shape wrong costs a second contract change. P2 because no image depends on it in this spec.

**Independent Test**: A test double producer fills a structure for each situation the renderer has today (camera grid, camera ray hit under each hit mode, secondary ray hit, shadow-type hit, photon hit, a bare point) and a checker validates the documented invariants; a table maps each existing situation to its ray type and hit mode and is checked against the code's present inference.

**Acceptance Scenarios**:

1. **Given** a batch of N points, **When** it is described, **Then** per-point fields (position, normals, parametric coordinates, derivatives, time, sample identity) and per-batch fields (object handle, ray type, hit mode, path depth, transport mode, shading-cell footprint, grid dimensions when the batch is a grid) are distinguished, and a batch of one is valid.
2. **Given** each situation in which the renderer shades today, **When** the mapping table is read, **Then** it names the ray type and hit mode the situation has, derived from the code (grid versus ray dimension, `camerahitmode`, the illumination hook, the photon hider), and a test reproduces the derivation for every situation the visual suite exercises (rows 6 and 7 of the table excepted: implicit-surface and DSO-object hits are asserted from `classifyShading()` alone, B8).
3. **Given** a field a producer cannot fill (a ray hit has no grid differences; a photon has no screen-space derivatives), **When** the structure is built, **Then** the contract says how absence is expressed and consumers are required to check it, rather than reading garbage.

---

### User Story 6 - Tracing goes through a Shading-owned interface (Priority: P2)

A developer testing a shader that traces, or writing a hider, supplies the answer to "trace these rays" through an interface Shading owns. Today the call is a member of the shading context whose body is in Hiders. After this feature `ITracer` is that interface; the existing hiders' code answers it unchanged, and a double answers it in tests.

**Why this priority**: V2(d), the upward capability that Principle VIII rule 2 requires to be inverted. P2 because hiders owning a context and implementing the interface is spec 029's, so this story defines and adapts. It removes only the `trace`/`traceEx` link edges its routing makes stale (Q1, plan-stage session); the bundle and photon-map edges stay.

**Independent Test**: With a tracer double that returns canned hits, run shaders using `trace()`, `transmission()`, `visibility()`, `gather()`, `occlusion()` and `indirectdiffuse()` and check what the double received (ray origin, direction, differentials, time, bias, ray type, depth, sample identity) and that the builtins consume the canned answers.

**Acceptance Scenarios**:

1. **Given** a tracer double, **When** the traced builtins run, **Then** the double receives every ray the builtin would have traced on master, in the same order, with the same sample identity, and the builtin's result is built from the double's answer.
2. **Given** the real renderer, **When** the same builtins run, **Then** the same rays are traced and the images are byte-identical to master's.
3. **Given** no tracer installed, **When** a traced builtin runs, **Then** it reports one warning for that kind and returns the value it returns for a ray that hits nothing, never a crash.

---

### User Story 7 - The registry reflects what moved (Priority: P3)

A maintainer reading `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` and `tests/architecture/allowlist.txt` sees DB-018's service part and the reduced part of DB-006 recorded, and the allowlist shrunk by exactly the edges that went.

**Why this priority**: Required by constitution rule 8 and by #32's acceptance, but mechanical once the code has moved.

**Independent Test**: Run `ctest --test-dir build -L architecture` on macOS and Linux; diff the allowlist and the registry against the starting tree.

**Acceptance Scenarios**:

1. **Given** the finished tree, **When** the architecture check runs, **Then** it passes with no `VIOLATION`, `STALE` or `BADID` line, and no allowlist line was added or re-keyed.
2. **Given** each allowlist line whose edge this spec removes, **When** the diff is read, **Then** the line is deleted, the entry's size and history record the spec number and date, and an entry with no line and no finding left is moved to Resolved.

---

### Edge Cases

- **No services installed** (an imager or default shader loaded before the first frame, a standalone library user): every v2 service falls back to the documented default and warns once per kind; nothing dereferences a missing implementation (spec 024's rule extended to the new services).
- **Name lookups that fail**: an attribute, option, message source or coordinate system that does not exist returns not-found and leaves the destination untouched, exactly as today; a type mismatch that cannot be copied safely is refused (FR-026); one that can be (a smaller stored value) keeps today's partial copy.
- **Varying versus uniform queries**: message passing is evaluated once per call with the first name (`name[0]`) and copied to every active lane; the services see one request per call, not one per lane. Active-lane masks and the uniform-conditional behaviour of the JIT (GitHub #8) are unchanged.
- **Mutable parameters**: `surface()`, `displacement()` and `atmosphere()` return a variable and a global index that the JIT uses to read the producing shader's current value (`jitParameterFinish`); `incident()` and `opposite()` skip mutable parameters. The v2 answer must carry the same information.
- **Light queries** (`lightsource()`): they read the light being iterated, which exists only inside an illuminance or illuminate loop; outside one, the result is not found and nothing is dereferenced (FR-028; master's interpreter dereferences a null light).
- **Thread safety**: services are called from every render thread. Each v2 service states whether it is called concurrently; the renderer's implementations take no new lock on the hot path (SC-010).
- **Re-entrancy**: a service called from inside a service (a tracer that shades, which re-enters `shade()` with a new state) must work, as `trace` does today through `traceEx` (shading depth).
- **A batch with inactive lanes or a derivative tail**: the contract states, as the grid code does today, that derivative-offset vertices follow the real ones and are not counted in the point count.
- **Ray type of a point shaded for something other than rendering** (displacement-only, bound estimation, a displacement pass): its ray type and hit mode are defined, not left to the producer.
- **Build options**: builds without the JIT keep building and passing; the `.slo` halves of the double-based tests register only with the JIT on. Builds without OpenEXR are unaffected.
- **Plug-in boundaries**: shadeops (`dso`) and DSO-visible structures are not touched; this boundary is internal C++ and not part of the C plug-in ABI of Principle VIII rule 4.

## Requirements *(mandatory)*

### Functional Requirements

**The v2 services interface**

- **FR-001**: Shading MUST own one renderer-services interface, version 2, that evolves the existing one in place (no second interface, no parallel old one): it keeps every capability of the current interface that Shading still needs and adds the services below. The interface MUST carry a version constant (major, minor, patch) from its first commit, with a documented rule for what bumps each part, and the history of changes MUST be recorded next to it. The constant is not checked at run time (there is no external implementer); its job is to make a change visible. Decided (Q9): one constant, `kRendererServicesVersion{2, 0, 0}`, covers the services, `ITracer` and `ShaderGlobals` (D22).
- **FR-002**: The interface MUST be documented in its header and in the spec's contract notes: for each method the inputs, outputs, not-found result, thread-safety, whether it may re-enter Shading and the version that introduced it. An implementer who reads only the documentation MUST be able to write a conforming double.
- **FR-003**: The interface MUST be implementable without any renderer-core, hider or geometry type in its signatures: every object, shader instance, light and ray reaches a service as an opaque handle or a Shading-owned plain structure. The interface MUST NOT include a header from Hiders, Geometry or the renderer core. This applies to the v2 members. `setOffendingObject` takes an `ObjectHandle`, and the one Hiders type left in a signature is `getPhotonMap()`'s `CPhotonMap *`, which FR-013 keeps for spec 032 (Q11).
- **FR-004**: The shading library MUST be usable with a standalone double of the interface (constitution rule 5). A double (or a small set) MUST exist in the tests that implements the whole interface with canned or table-driven answers, records every call, and has no inner implementation to forward to. The four existing whole-interface classes (the forwarding wrappers `CForwardingServices`, `CRecordingServices` and `CTextureServices`, and `CStubServices`, which answers fixed sentinels) MAY be kept.

**Attributes and message passing (V15 step 1, DB-018)**

- **FR-005**: `get_attribute` MUST answer `attribute()` and `option()` queries (and `rendererinfo()` where it reads the same data): the request names the shading point's object handle, the attribute or option name and the type the shader expects, and the answer is a raw typed value and an explicit status (found, not found, type mismatch); the type conversion and refusal master applies stay in Shading and run on that value. The set of names it knows MUST be the set `CShadingContext::attributes()` and `options()` know on master, plus user attributes and options as today. `TypeMismatch` is reported by the service where the stored value cannot be copied safely into the expected type, and Shading refuses it (FR-026; this supersedes Q4's "treated as `Found`"). A query with no current object (a null `ObjectHandle`) answers `NotFound` (Q12, FR-027).
- **FR-006**: `getmessage` MUST answer `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()` and `lightsource()`: the request names the source (the shader type attached to the object, or the current light), the parameter name and the expected type; the answer carries the value, the same explicit status (the service decides `TypeMismatch` before it writes anything into the destination, FR-026) and what the JIT needs to read a mutable parameter (variable and global index) exactly as `surfaceParameter` and its siblings return them today. The split is fixed (Clarifications, 2026-10-30): `get_attribute` carries `attribute()`, `option()` and `rendererinfo()`; `getmessage` carries only these six shader-to-shader queries.
- **FR-007**: The interpreter builtins MUST call the services for the builtins of FR-005 and FR-006, and so MUST the JIT: because both backends reach these through the same `CShadingContext` members, the change MUST be made inside those members (or the helpers they call), so that `.rslo` and `.slo` issue identical requests without an emitter change. Exception (Q2): `lightsource()` reaches no shared member. The interpreter's macro and the JIT's `op_lightsource_f` each go through `getMessage` and keep their own light, variable request and result handling, so each backend's output is unchanged. The divergence is recorded, not fixed. Spec 033 keeps ownership of reshaping the JIT's message-passing entry points. `setmessage` and group-layer messages are not part of this spec (spec 034 adds them as a backward-compatible addition).

**Coordinate systems (`get_matrix`)**

- **FR-008**: `get_matrix` MUST return the from and to matrices for a coordinate-system name, given the shading point's object handle, the executing shader instance's handle, the current light's handle and a time. It replaces both `findCoordinateSystemWithType` and the object/shader/light resolution done inside Shading. The time parameter MUST be part of the contract (motion-blurred transforms, OSL), and the renderer's implementation MUST return the same matrix for every time, because master does; the time is carried in the signature only, and a test double may honour it; honouring it in the renderer is out of scope (see Assumptions). Shading's call sites carry no time and pass 0, the normalised shutter-open time of `CRay::time` (Q14).
- **FR-009**: All call sites of `findCoordinateSystem` (about 30) in the shading library MUST go through `get_matrix`. The diagnostics for an unknown system, an object reference without an object, and the colour-space names handled by the same lookup MUST stay as they are. **Amended 2026-10-31 (D8, maintainer):** the emitter gains one fix in this spec. The JIT lowered the two-space `ctransform("from", "to", c)` (`c=SSc`) as if it were the one-space form, so it asked for one name and read the second name as the colour. `llvmEmitter.cpp` now dispatches the `ctransform` branch on the proto shape, as the `transform` family does, and calls a new `op_ctransform_ss` (`rslOps.cpp`, near `op_ctransform`) that resolves both names through `jitFindCoordinateSystem`. This adds one `op_*` function and changes no existing signature. Without it `Matrices{Jit}` and `RealAnswers{Jit}_Matrices` cannot equal the interpreter. **Second emitter change, D9 (maintainer 2026-11-01, Phase 5):** the branch of `llvmEmitter.cpp` for `surface`/`displacement`/`atmosphere`/`incident`/`opposite`/`attribute`/`option`/`rendererinfo` now maps the color, normal and point prototypes (`f=SC`, `f=SN`, `f=SP`) to the triple `resultKind` 3, like `f=SV`, as the interpreter's `DEFLINKFUNC` does. No `op_*` function is added or changed.

**Tracing (`ITracer`, V2(d))**

- **FR-010**: Shading MUST own an `ITracer` interface and reach it through the v2 services. It MUST be able to carry every request Shading issues today: a single ray that is not shaded (`trace(CRay *)`) and a bundle of rays that is shaded and post-processed (`traceEx(CRayBundle *)`), including the shading depth, the label, the post-shade callbacks and the sample identity each ray carries. The interface is expressed in the Shading-owned types that exist today (`CRay`, `CRayBundle`), and Shading's call sites go through it. It is installed per frame through the services (registered in `beginFrame()`, cleared in `endFrame()`, the calling context passed as a parameter); the plan MUST confirm that hiders running concurrently in one frame (the photon and camera passes) cannot overwrite each other. The hiders' existing trace code answers it unchanged behind an adapter. Bundle creation and the photon-map lookup are not moved here (spec 029). The routing leaves DB-001's three `trace`/`traceEx` link lines stale, and they are deleted (8 -> 5 lines, 6 -> 4 imports). No DB-005 line moves (Q1). The OSL-shaped single-ray request is spec 049's adapter over this interface (Clarifications, 2026-10-30).
- **FR-011**: Every traced builtin (`trace`, `transmission`, `visibility`, `gather`, `occlusion`, `indirectdiffuse`, and the ray-traced forms of `environment` and `shadow`) MUST issue its rays through the tracer service, in the same order and with the same per-ray sample identity, depth, time, bias and differentials as on master, in both backends. The rays of `occlusion`/`indirectdiffuse` are cast by the hiders' irradiance cache (`irradianceDispatch.cpp`), whose two trace calls also go through the tracer helper (Q6).
- **FR-012**: With no tracer installed, each traced builtin MUST warn once per kind and return the value a ray that hits nothing yields. A tracer MUST be allowed to re-enter Shading (it shades what it hits), as `traceEx` does.

**Texture and the other existing services**

- **FR-013**: The texture part of the v2 interface MUST be spec 027's, adopted unchanged: `textureSystem()` and the 3D-channel forwarders. This spec MUST NOT add a texture service of its own or change a texture signature. `getPhotonMap`, `getCache` and `getTexture3d` are not changed (spec 032).
- **FR-014**: Everything else in today's interface (frame and option getters, the illumination hook, filter/DSO/variable lookup, global IDs, offending-object reporting) MUST keep working with the same behaviour. The 57 methods (56 before the plan-stage recount) are not regrouped in this spec: the v2 services are added as new members. The plan MUST classify each of the 57 methods as unchanged, grouped under the v2 services, or removed because a v2 service replaces it, and say why; a method is removed only when nothing in the tree calls it afterward. Decided (Q3): 28 unchanged, 1 replaced (`setOffendingObject`), 28 removed (`findCoordinateSystem`, `findCoordinateSystemWithType`, the 25 option getters, `lookupUserOption`); the option name table moves into the renderer's `getAttribute` (plan `contracts/renderer-services-v2.md`).

**ShaderGlobals, ray type and hit mode (V3 step 1, V9 step 2)**

- **FR-015**: Shading MUST define `ShaderGlobals`, the description of a batch of shading points. It MUST be structure-of-arrays with a runtime point count and no fixed batch width (each per-point field a pointer or span plus a stride), and batch- or grid-shaped (a grid of u by v points, a span of independent points, or one point), so that a REYES grid and an OSL batch of 4, 8 or 16 map onto it without a later contract change. It MUST carry, per point: position, shading normal and geometric normal, the incident direction when there is one, parametric coordinates, the surface derivatives (`dPdu`, `dPdv`), the point's differentials (`dPdx`-style for ray hits and the grid's `du`/`dv` differences for grids), time and its derivative with respect to time (`dPdtime`), and the sample identity (spec 039's key). Per batch: an opaque object handle, an opaque shader-instance handle, the ray type, the hit mode, the path depth, the transport mode (radiance or importance) and the shading-cell footprint (the square root of the shading rate, in pixels, the same rule in every hider).
- **FR-016**: `ShaderGlobals` MUST state how absence of a field is expressed (for example no screen-space derivatives for a photon, no grid differences for a ray hit) and which fields a consumer may assume. The contract MUST define the derivative-offset tail and inactive lanes exactly as the grid code treats them today.
- **FR-017**: The ray type MUST be an explicit enumeration that names at least the camera ray, the secondary shading ray (reflection, refraction, gather, occlusion, indirect), the shadow or visibility ray, the photon (importance) ray and "no ray" (a grid shaded by a rasterizing hider, a bare point); the hit mode MUST name at least "shade the surface" and "primitive only" (today's `camerahitmode` values `s` and `p`). A table in the contract MUST map every situation in which the renderer shades today to its ray type, hit mode and transport mode, with the code location that determines it. The enumerations MUST allow additions as backward-compatible changes. Decided (Q10): `RayType` has five values, `None`, `Camera`, `Secondary`, `Shadow` and `Photon`.
- **FR-018**: Nothing in the render path MUST produce or consume `ShaderGlobals`, the ray type or the hit mode in this spec: `shade()` keeps taking the primitive, the illumination hook and `camerahitmode` keep working as they do. Spec 029 switches `shade()` to the structure and reads the ray type. Tests MUST exercise the structure through a double producer and a double consumer, and MUST check the FR-017 table against the code's present inference. The rows that are oddities or that no `shade()` call reaches (rows 6, 7 and 10 to 14) are asserted from `classifyShading()` alone; the others are rendered.

**Registry and guardrails**

- **FR-019**: The architecture check MUST pass on macOS and Linux. The allowlist MUST only shrink: every line whose edge this spec removes is deleted, and no line is added or re-keyed. The plan MUST list, from a measurement on the starting tree, each line expected to go. DB-006's Shading-origin include edges (`execute.cpp` and `executeMisc.cpp` to `surface.h`) are expected to go once the attribute and matrix reads that need the primitive's type have moved behind the services; `shading.cpp` to `object.h` stays for spec 029's `shade()` switch. If the plan finds an edge cannot go, it records why.
- **FR-020**: `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` MUST be updated: DB-018's evidence, size and direction are edited to say the service part is done (with the spec number and date), what remains (the JIT's message-passing entry points, spec 033; group-layer messages, spec 034) stays Open under the entry; DB-006's counts and history record the reduction; DB-001's evidence, size and history record the three deleted `trace`/`traceEx` lines (8 -> 5, Q1), and it stays Open; DB-005 is not edited. An entry moves to Resolved only when it has no allowlist line and no finding left. No registry text, task or success criterion may refer to anything that exists only after the merge.
- **FR-021**: The renderer-core usage-metric baseline (`tests/architecture/metrics-baseline.txt`) MUST be lowered to the measured end values if any reference to the renderer core goes, and the Shading count MUST NOT rise.

**Behaviour and tests**

- **FR-022**: Rendering output MUST NOT change: byte-identical on macOS for every scene, `.rslo` and `.slo`, and the same pass/fail set as master on each platform. The one exception is the set of paths of FR-026 to FR-028, which on master overflowed a destination, mixed a string with a number, or dereferenced a null object or light; no scene of the suite is expected to reach one, and the byte comparison at every phase is what proves it. A difference there stops the phase for the maintainer. If the plan finds a place where the current behaviour differs between the two backends for a builtin this spec routes, it records the difference and keeps each backend's output (this spec fixes none).
- **FR-023**: Tests MUST be written first and shown failing on the tree before the change (constitution III): (a) a program that links the shading library, `libRIBCommon` and the double and no renderer object (User Story 1); (b) the attribute and message test under both backends against canned answers (User Story 3); (c) the `get_matrix` test (User Story 4); (d) the `ShaderGlobals` producer/consumer and situation-table test (User Story 5); (e) the tracer test (User Story 6); (f) a no-services test per new service (four tests, QB); (g) the type-safety, null-object and no-light tests of FR-026 to FR-028, each written in the phase that fixes it and approved at that phase's TEST-APPROVAL (QF). Existing tests MUST keep passing without edits beyond include paths and the doubles' base class. Allowed beyond that: deleting the overrides of the 28 removed methods, and changing `setOffendingObject`'s parameter, in the four whole-interface doubles (Q3); trailing fields in the two spec-039 observer reports; and, in the three doubles that forward to an inner implementation (`CForwardingServices`, `CRecordingServices`, `CTextureServices`), one forwarding line for each of `getMatrix`, `getAttribute`, `getMessage` and `tracer()`, added in the phase that makes the member live (QA). The replay tests (SC-005) are characterization tests: green by construction on the starting tree, shown red by a per-builtin mutation (Q7).
- **FR-024**: The standalone-double test program MUST NOT link `ribRender`. Where `libshader_shading` still carries flat-namespace imports that dyld binds at load (the bundle vtables, `CGatherBundle`'s constructor, `CPhotonMap::lookup`), the test program MUST supply them from a stub that lives in the test directory and that the plan names symbol by symbol, or the imports MUST be gone; the plan measures which symbols a covered path actually needs on macOS and Linux. The stub is deleted by the spec that removes the imports (029). A test that depends on the stub being exercised MUST fail, not hang.
- **FR-025**: No task or text MAY depend on the merge. Issues close through the maintainer's merge message (`Closes #32`); this spec does not write it.

**Findings resolved in this spec (triage of 2026-10-31; `validation.md` §4)**

- **FR-026**: The copy of a stored value into a shader's destination MUST NOT write past the destination, nor reinterpret a string as a number or the converse, where the builtin can tell. The service reports `TypeMismatch` when the expected type is `Triple` and the stored value has more than 3 floats, when it is `Matrix` and more than 16, when it is `String` and the stored value is not a string, and when it is any other type and the stored value is a string. For `getMessage` the service MUST decide before it writes the default value into `dest`. Shading MUST then leave the destination untouched, return 0 (not found) and warn once per builtin kind per process (with a code that is not swallowed). A stored value smaller than the destination is copied as on master. This applies to `attribute()`, `option()`, `rendererinfo()`, `surface()`, `displacement()`, `atmosphere()`, `incident()` and `opposite()` under both backends. It does **not** apply to source `Light` (`lightsource()`): the two backends disagree there about which light is read and how the result is returned (Q2, O1), and FR-022 forbids changing either output. The destination size of a float-class query is not known to the builtin (RSL passes no length), so a stored multi-float value asked for as a scalar `float` is still copied in full; that residual is filed (O7). Supersedes Q4.
- **FR-027**: A query with no current object MUST answer not found and MUST NOT dereference a null object: `attribute()`, `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()` (the service answers `NotFound` for a null `ObjectHandle`) and `shadername(type)` (the empty string). `option()` and `rendererinfo()` do not read the object and answer as usual. Q12, extended; Q8's direct read of `shadername(type)` stays, null-guarded.
- **FR-028**: `lightsource()` called outside an `illuminance` or `illuminate` loop MUST answer not found and MUST NOT dereference a null light under the interpreter; the JIT already does. Each backend's output inside a loop is unchanged (Q2).
- **FR-029**: The accessor constants that the interpreter's `incident()` and `opposite()` pass to `PARAMETEREXPR_PRE` MUST agree with the source Shading asks for (`Interior` and `Exterior`), and so MUST the accessors that `jitIncidentParameter` and `jitOppositeParameter` pass to `jitParameterFinish`. The accessor is never read for these two (no variable comes back), so no output changes.

### Key Entities

- **Renderer services v2**: the Shading-owned interface through which the shading library obtains everything it does not own, with a version constant. Evolves today's `CRendererServices`.
- **`get_attribute`**: the service answering attribute, option and renderer-info queries for a shading point's object.
- **`getmessage`**: the service answering shader-to-shader queries (the shader of a given type on the object, the current light).
- **`get_matrix`**: the service answering named coordinate-system matrices, for a point's object, shader instance, light and time.
- **`ITracer`**: the Shading-owned interface for tracing rays and ray bundles; implemented today by the hiders' code and by a double in tests.
- **`ShaderGlobals`**: a batch of shading points with per-point and per-batch fields, the ray type, the hit mode, the path depth, the transport mode and the shading-cell footprint.
- **Ray type / hit mode**: explicit enumerations replacing today's inference.
- **Services double**: a standalone implementation of the interface for tests, with canned answers and call recording.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A test program that links the shading library, `libRIBCommon` and the services double, and no renderer-core, hider or geometry object, builds, installs the double and passes under the interpreter and (JIT on) the JIT on macOS and Linux (User Story 1, FR-004, FR-024). The JIT half loads `.slo` through `parseSloShader()`, which moves into Shading (`sloLoader.{h,cpp}`) for this (Q5). `gather()`, the answers consumed through a bundle, the no-tracer bundle forms and the irradiance cache are covered by a second, `ribRender`-linked program (Q6).
- **SC-002**: Zero reads of the current object's attributes remain for `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()`, `lightsource()`, `attribute()` and `option()`, and zero uses of the primitive's transform remain for coordinate-system lookup; the plan lists the other attribute reads that stay and why (User Stories 3 and 4). `shadername(type)` is one of them: it stays a direct read (Q8).
- **SC-003**: For each of the eight message and attribute builtins, an interpreted and a JIT-compiled shader send the double identical requests and receive its canned answers, except `lightsource()`, whose documented backend difference is asserted instead (Q2); the same shaders on the real renderer return master's values (User Story 3).
- **SC-004**: Every coordinate-system lookup goes through `get_matrix`; the transform builtins' results equal master's for every system kind (User Story 4).
- **SC-005**: A tracer double receives, for each traced builtin, the same ray sequence master traces under both backends (User Story 6). This is checked by a single-threaded replay, recorded on the starting tree through the null-in-production `CShadingContext::traceRequestObserver` (Q7) as in spec 028's motion replay.
- **SC-006**: The `ShaderGlobals` situation table covers every shading situation in the visual suite (rows 6 and 7 excepted: classify-only, B8), and the test that reproduces the derivation passes; every field of the structure has documented presence rules (User Story 5).
- **SC-007**: On macOS, every scene of the visual suite, the parity suites and the determinism sweep renders to bytes identical to master's, and on each platform the full suite has master's pass/fail set (User Story 2, FR-022).
- **SC-008**: The architecture check passes on macOS and Linux; DB-006's allowlist lines fall by the measured count (the two Shading-origin `surface.h` include edges are expected), no line is added or re-keyed, and `Architecture_DomainBoundaries` reports no stale line (User Story 7).
- **SC-009**: `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` records DB-018's service part and DB-006's reduction with the spec number and date, and no entry refers to the merge.
- **SC-010**: A shading-heavy scene's render time on the finished tree is within the run-to-run noise measured on master, on the same machine and thread count (the services add a call where a field read was; a regression beyond noise is a defect, as in spec 027).
- **SC-011**: Shading's flat-namespace imports on macOS do not increase and are a subset of the starting set; the Linux counterpart does not grow.
- **SC-012**: The interface headers contain the version constant, the per-method documentation of FR-002 and the version history, and a reader who has seen only them can write a conforming double (reviewed against the test double by someone who did not write it).
- **SC-013**: For every `TypeMismatch` case of FR-026, a test with a canary beside the destination shows no byte written, the builtin returning 0 and one warning, for the eight builtins under both backends, against the standalone double and against the real renderer (User Story 3 AS4). The accessor constants of FR-029 agree with the sources, in the interpreter and in the JIT.
- **SC-014**: With no current object, the six object builtins (`attribute`, `surface`, `displacement`, `atmosphere`, `incident`, `opposite`) and `shadername(type)` answer not found while `option()` and `rendererinfo()` answer as usual, and `lightsource()` outside a loop answers not found, under both backends and against the real renderer, with no crash (User Story 3 AS5 and AS6; FR-027, FR-028).

## Scope Boundaries

**In scope (026)**: the v2 services interface and its version; `get_attribute`, `getmessage`, `get_matrix` and the trace service; `ITracer`; `ShaderGlobals`, ray type, hit mode and transport mode as definitions; the adapter of the existing implementation behind each; the standalone double; registry and allowlist edits.

**Out of scope, and whose it is**:

- **Spec 029 (hider/shading separation)**: hiders owning a `ShadingContext` instead of inheriting it, hiders implementing `ITracer` themselves, `trace*` moving out of Shading, `shade()` consuming `ShaderGlobals` (and `CSurface::fillShaderGlobals`), the explicit ray type being read by the hiders and the shading code, deleting DB-001's remaining edges and DB-005, linking with `-undefined error`, removing the `-force_load` workaround. This spec only defines the structure and the interface.
- **Spec 031 (geometry hider-agnostic)**: Geometry's `IDicer`/`IRayContext`, the Dispatch/Stub pairs, and DB-006's Geometry-origin edges (the 30 lines that are not Shading's).
- **Spec 033 (shading-system backends)**: the JIT's message-passing entry points and `ShadingSystem`/`IShaderBackend`; this spec changes the `CShadingContext` members they call, not the emitter.
- **Spec 034 (shader networks)**: `setmessage`, group-layer messages and lazy layers (added to the interface later as a backward-compatible change).
- **Spec 032 (GI cache split)**: photon-map, irradiance-cache and 3D-texture getters.
- **Spec 049 (OSL backend) and 052 (public runtime library)**: the OSL `RendererServices` adapter and the public C facade; this interface stays internal C++ and is shaped so that they can be written over it.
- **Spec 054 (shading-rate unification)**: the shading-cell footprint is a field of `ShaderGlobals` and is filled with today's value (the shading rate's square root as `shade()` computes it); changing the rule is 054's.
- **The out-of-scope findings O1 to O7** (`validation.md` §4): the JIT/interpreter `lightsource()` divergence (spec 033), `CDLObject::shade`'s `vVertices = -1` and the 0-D implicit-surface hits (spec 031), `camerahitmode`/`diffusehitmode` (spec 029), the behaviours to verify against the RISpec, and the unknown destination size of a float-class query (spec 033). They are filed as #115 to #121 (P1); evidence is added as a comment on the issue.
- **Motion-blurred transforms and ray differentials as new features**: the time parameter and the differential fields exist in the contract; the implementation returns and fills what master does.

## Assumptions

- Specs 023, 024, 025, 027, 028 and 039 are merged on master (`d01da47`). The base of this spec is master; no rebase is assumed.
- The interfaces are internal C++ (plan decision, "026 stays internal C++"). Principle VIII rule 4's MAJOR/MINOR/PATCH discipline applies to plug-in boundaries, so there is no run-time version check; the constant exists so that a change to the contract is a visible, documented act. The public C facade is spec 052.
- The services interface evolves in place (one `CRendererServices`, version 2). It has one production implementer and four test classes (three forwarding wrappers and a sentinel stub), all in this repository, so no parallel old interface is kept. The names `get_matrix`, `get_attribute` and `getmessage` follow OSL's `RendererServices` for the adapter of spec 049; the exact C++ spellings, argument types and where the interface lives are the plan's decision.
- Because the interpreter and the JIT call the same `CShadingContext` members for message passing, routing those members through the services reaches both backends. Spec 033's later reshaping of the JIT entry points is unaffected.
- `ShaderGlobals` is a contract, not yet a data path (FR-018). Its field list follows the plan's brief (batch-shaped, ray differentials and grid differences, path depth, transport mode, shading-cell footprint) and reference document sections 5.5, 8.1 and 8.3. The layout is structure-of-arrays with a runtime point count (Clarifications, 2026-10-30); the allocation and the exact spellings are the plan's decisions.
- The path depth, the sample identity and the transport mode already exist in the renderer (`CRay::depth`, spec 039's key, the photon hider); this spec names them in the contract and does not add data.
- The cameras' and hiders' existing inference of the hit mode (`camerahitmode`, read only for grids) is preserved as the definition of today's behaviour, including its oddities; any oddity found while writing the situation table is recorded, not fixed, and filed (O2 to O6, `validation.md` §4).
- The current object, the shader instance and the light reach the services as opaque handles that Shading passes through unchanged; the renderer-core implementation converts them to its own types. Shading continues to hold a `CSurface *` until spec 029 (DB-006's `shading.cpp` to `object.h` edge).
- Texture, 3D-texture channel and `textureSystem()` access stay exactly as spec 027 left them (FR-013). Spec 027 defined the texture part of the interface; this spec adopts it.
- DB-018 has no allowlist line (it is not a dependency edge), so "allowlist lines of every resolved entry deleted" is met through DB-006's lines, and DB-018 stays Open for the JIT and group-layer parts; DB-001 loses its three `trace`/`traceEx` lines (Q1), and DB-005 loses none.
- Out of scope and already filed: the JIT's `printf()` is a no-op dispatch case (GitHub #11), a JIT-wide uniform-conditional defect (GitHub #8), `setcomp`/`comp` index inconsistency (#9), and `transform()`'s matrix overload (#10).
- Linux is validated by the agent over ssh (`juvenal@ubuntu-2404-desktop.v2-labs.io`) in the per-spec tree `~/Projects/Development/CLI/openrender-026`, with the remote `cpujob` at `-j2` (Q13). The Linux baseline is a gate before Phase 2 (plan quickstart §4).

## Open Questions

None remain; all questions were answered on 2026-10-30 (see Clarifications).
