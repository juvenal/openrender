# Research: Shading System Backends (spec 033)

Phase 0 of the plan. Every item is a decision with its rationale and the alternatives weighed. Items marked
**(maintainer)** were listed in plan.md "Decisions asked of the maintainer" and were all approved as recommended on 2026-11-14 (plan.md "Decisions approved"); every other item
is a plan-level choice a reviewer can overrule.

Starting tree: master `278b0394`, worktree `033-shading-system-backends`. Measured on macOS (Apple clang, vcpkg,
Release, `-j4` through `cpujob`) on 2026-11-14 unless a line says otherwise.

---

## R0. Measurements and corrections to the spec and the registry

### R0.1 Measured at the plan stage

| What | Result | How |
|---|---|---|
| Base build (JIT on) | builds, `exit 0`; only the known linker notices (deployment target 13.0 vs 13.3, duplicate static libraries) | `cpujob 033-plan-base-build cmake --build build -j4` (configure 7 s, build about 2.5 min) |
| JIT-off build (`OPENRENDER_ENABLE_JIT=OFF`) | builds, `exit 0`, 0 errors; 137 `.rslo` and no `.slo` in `build-nojit/shaders` (FR-020's starting point is green) | `cpujob 033-plan-nojit-build cmake --build build-nojit -j4` (about 2.4 min) |
| `archcheck` | pass; `CONSUMER` 71 lines; `METRIC` TOTAL 491 refs / 71 distinct, Shading 0/0, Hiders 489/69, Geometry 2/2 | `cpujob 033-plan-archcheck build/tests/architecture/archcheck --source-dir . --build-dir build --config tests/architecture` |
| `sloinfo` CONSUMER edges | 3: `src/libshader/runtime/rslo.h`, `src/libshader/shading/llvmJit.h`, `src/libshader/shading/sloMetadata.h` | same report |
| `oshader` CONSUMER edges | 3, all compiler headers: `compiler/llvmEmitter.h`, `compiler/opcodes.h`, `compiler/rslo.h`; **0 reach the shading engine** | same report |
| Core references in the bind-time init | the preprocessed body of `CRendererContext::init` (`init.cpp`, build flags, `-E -P`) holds **19** `CRenderer::findCoordinateSystem` calls (the 19 macro sites of #134) and **1** `CRenderer::retrieveVariable`; no other `CRenderer`/`CRendererContext` member is reached (identifier intersection with `renderer.h`/`rendererContext.h`) | `c++ $(flags of ribRender_obj) -E -P src/ri/render/init.cpp` |
| Loading benchmark (`tests/benchmark_loading.sh`, 100 `sloinfo` runs) | `plastic.rslo` 0.401 / 0.405 / 0.407 s; `plastic.slo` 0.436 / 0.415 s; one cold first run 1.131 s (discarded) | `cpujob 033-plan-loadbench bash tests/benchmark_loading.sh build/src/sloinfo/sloinfo build/shaders/plastic.{rslo,slo}` |
| Disk | 18 GiB free on `/Volumes/Projects` | `df -h` |

The benchmark measures `sloinfo` process start-up plus one metadata read; it never touches the renderer's load path.
R12 defines the method used for FR-024.

### R0.2 Corrections (recorded, applied where this spec writes the text)

| Where | Says | The code says |
|---|---|---|
| spec FR-015, A-7; CLAUDE.md gotcha 28 | `kRendererServicesVersion` is 2.1.0 | **2.3.0** (`rendererServices.h:131`; 2.2.0 spec 032, 2.3.0 spec 031). A step by this spec is **2.4.0** (R5). |
| DB-007 evidence | `rendererFiles.cpp:48,130,693-716`; `shader.h:157,223` | `rendererFiles.cpp:50` (bare `parseShader` declaration), `:52` (`sloLoader.h`), `:372-439` (`CRenderer::getShader`); precedence in `rendererContext.cpp:304-307`; JIT fields `shader.h:156-159` (`CShader`) and `:240-243` (`CProgrammableShaderInstance`); the switch `execute.cpp:502` |
| DB-018 evidence | `op_*_param` open | The eight `op_*_param` entry points already reach the services (`rslOps.cpp:2219-2272` -> `CShadingContext::jit*Parameter` -> `queryMessage`). Open: `op_lightsource_f`'s `getParameterShape()` read (`rslOps.cpp:938`) and `shaderName(type)`'s attribute reads (`shading.cpp:2907-2939`). |
| DB-024 | "`sloinfo.cpp` and `oshader.cpp` include Shading internals (3 each)" | sloinfo 3 (above); oshader's 3 are its own compiler's headers. A-5 settles: **nothing of oshader is this spec's**; spec 043 owns the compiler library. |
| FR-021 "the core's baseline only falls" | — | `metrics-baseline.txt` has no Core row (the metric counts other classes' references to the core). Read as: every class row stays at or below its baseline; this spec moves no reference into a domain (R13). |
| OSHADER_UPDATES.md "Shader format selection" | `OPENRENDER_DEFAULT_FORMAT` is an "env var or CMake cache" | It is a literal `#define OPENRENDER_DEFAULT_FORMAT "rslo"` in `config-cmake.h.in:37`; no environment variable or cache entry reads it. Corrected in the FR-027 doc update. |
| spec "Measured starting point", `CRendererContext::getShader` | `:298-345` | `:298-350` (the `setParameters` call is at :347) |

---

## R1. Where the facade lives and how long it lives

**Decision**: `ShadingSystem` is a Shading class (`src/libshader/shading/shadingSystem.{h,cpp}`). The renderer core
creates one in `CRenderer::initFiles()` (today's `globalFiles = new CTrie<...>`, run from `beginRenderer()` at `RiBegin`)
and deletes it in `shutdownFiles()` (today's `globalFiles->destroy()`, at `RiEnd`). The core holds it in
`CRenderer::shadingSystem` (a static pointer replacing `globalFiles`); nothing in Shading reaches it through a global.

**Rationale**: FR-005 asks for the cache's present lifetime; `globalFiles` holds only shaders (its single `insert` is
`rendererFiles.cpp:434`), so the swap is exact. Shading code never needs the facade while shading (instances carry their
backend, R3), so no process-wide accessor is needed, and a test builds its own system with doubles (FR-001).

**Alternatives**: a process-wide default installed like `CShadingContext::setDefaultServices()` (rejected: a second
global for no caller); one system per frame (rejected: the cache survives frames today).

## R2. Backends, registration and the format order

**Decision**:
- `IShaderBackend` (Shading) is a stateless object per format: `key()` (`"rslo"`, `"slo"`), `extension()` (the
  extension handed to the resolver), `load()`, `instantiate()`, `initialise()`, `execute()`. The two built-ins are
  singletons defined in their own translation units (`rsloBackend.cpp`, `sloBackend.cpp`) and registered by a
  self-registering static registrar into a function-local registry (`shaderBackendRegistry()`, Meyers singleton), with a
  priority: rslo 0, slo 10, others later. `libshader_shading` is a shared library, so its static initializers always run
  (the static-archive caveat of gotcha 31 does not apply). A test registers its own backend with
  `registerShaderBackend()` before it builds a `ShadingSystem`, and removes it after.
- In a JIT-less build `sloBackend.cpp` compiles its `#else` branch, which registers the key `"slo"` as **unavailable**,
  with today's warning text.
- The facade's order for an uncached name: the registered entry whose key equals the resolved preference first (if
  any), then every other entry by priority. For each entry: an unavailable entry warns (only when it is the preferred
  one) and is skipped; an available one asks the resolver for `name` with its extension and, when found, calls `load()`;
  the first non-null description wins.

**Rationale**: reproduces today exactly. Preference `"slo"` with the JIT: slo then rslo; anything else: rslo then slo
(today's test is `strcmp(preferredType, "slo") == 0`). JIT-less with `"slo"`: the `CODE_INCAPABLE` warning, then rslo,
once per **uncached request** (the warning sits in the not-cached branch, `rendererFiles.cpp:404`); JIT-less with
`"rslo"`: rslo only, no warning. A third backend (test, later OSL, spec 049) joins after the two RSL ones and is reached
first only when the preference names its key, so production search is unchanged (D9 unchanged: `Surface` stays RSL;
049 adds `.oso` after `.slo`/`.rslo`). No comparison chain in the facade (FR-002).

**Alternatives**: explicit registration in the facade's constructor (rejected: adding 049 would edit the facade);
a `bool available()` on a JIT backend compiled without LLVM (rejected: the backend needs LLVM headers).

## R3. Description, payload and instance (FR-007, FR-008)

**Decision**:
- `CShader` **keeps its name** and becomes the backend-neutral description: `name` (from `CFileResource`), `type`,
  `parameters`, `usedParameters`, `flags`, `data`, plus `const IShaderBackend *backend` and `CShaderPayload *payload`
  (owned; deleted by `~CShader`). It stays a `CFileResource` (the cache's element type).
- Payloads: `CRslPayload` (the RSL frame layout both RSL backends share: `numVariables`, `varyingSizes`, `numGlobals`,
  the owned `strings`/`numStrings`/`memory`); `CRsloPayload : CRslPayload` adds `codeArea`, `constantEntries`,
  `codeEntryPoint`, `initEntryPoint`; `CSloPayload : CRslPayload` adds `jitEntry`, `jitInitEntry` (the LLVM type
  `TShaderJitEntry` appears only in `sloBackend.cpp`/`shaderPayload.h` under the guard).
- `CProgrammableShaderInstance` stays the RSL instance (both RSL backends), with its constructor signature
  `(CShader *, CAttributes *, CXform *)`; it loses its `jitEntry`/`jitInitEntry` copies. `prepare()` reads the RSL frame
  layout from the payload.
- The loaders keep their names and signatures (`CShader *parseShader(const char *, const char *)` in `rslo.y`,
  `parseSloShader` in `sloLoader.{h,cpp}`) and set `backend`/`payload` themselves, so a description built outside a
  `ShadingSystem` (the contract harness) still runs.
- `shader.h` stops including `llvmJit.h`: no LLVM type reaches a core translation unit any more.

**Rationale**: approved tests read `prog->parent->type`, `->usedParameters`, `->parameters`
(`test_imager_options.cpp:27`, `test_ol_wiring.cpp:124`, `test_used_parameters_oracle.cpp:141`,
`test_slo_default_buffer.cpp:263`), build `new CProgrammableShaderInstance(shader, attributes, xform)`
(`contract_harness.h:82`) and call the two loaders by name; keeping those names keeps every approved test compiling
unchanged (R14, P-Q6). The core reads none of the moved fields after Phase 3.

**Dispatch invariant** (stated in contracts/shader-backend.md): today the switch `jitEntry != nullptr` is equivalent to
"loaded by `parseSloShader`" because `jitEntry` gets a value only in `sloLoader.cpp` (shader.cpp only nulls or copies it)
and `parseSloShader` nulls `codeArea`; and `codeArea == nullptr && jitEntry == nullptr` is exactly "a `.slo` whose JIT
compile failed". Dispatching on the loader's backend is therefore the same switch.

**Alternatives**: rename `CShader` to `ShaderDescription` (rejected: churn in every test and 30 files for a name);
keep `numVariables`/`varyingSizes` in the description (rejected: FR-007 lists the description's fields; the frame layout
is execution detail of RSL).

## R4. Execution dispatch (FR-010)

**Decision**: `CShadingContext::execute` keeps its name and becomes the dispatcher, with the **neutral** signature
`execute(CShaderInstance *, float **)` (today `CProgrammableShaderInstance *`): it calls
`instance->shaderDescription()->backend->execute(*this, *instance, locals)`. Every existing caller passes a
`CProgrammableShaderInstance *`, which converts implicitly, so every approved caller compiles unchanged (P-Q6), and a test
or future backend's own instance class (the T028 third backend) reaches the dispatcher. The bytecode loop moves to a new
public member `CShadingContext::interpret(CProgrammableShaderInstance *, float **)` (the body of today's `execute`
without the `#ifdef OPENRENDER_HAVE_LLVM` block); the interpreter backend's `execute` `static_cast`s the instance to
`CProgrammableShaderInstance *` and calls it. The JIT block moves
verbatim into `sloBackend.cpp` (`CSloBackend::execute`): set `currentShaderInstance`, build `stuff[]`, save
`activeContext`, set it, call `jitEntry`, restore it, the light's ambient-`Cl` accumulation, and the
"has no bytecode and no JIT entry" `CODE_SYSTEM` warning when `jitEntry` is null. `CProgrammableShaderInstance::execute`
and `::illuminate` keep calling `context->execute(this, locals)`.

**Rationale**: eight approved tests call `context->execute(instance, locals)` with either format
(`contract_harness.h:134`, `case_volume_segments.cpp:191`, `test_illumination_hook*.cpp`, ...), and `RSLShading::shade()`
calls it too; keeping the dispatcher under the old name keeps all of them and `RSLShading` unchanged (A-6: it forwards,
same effect). No new friend of `CShadingContext` (`tests/link/check_scope.cmake` S1c allows only
`CProgrammableShaderInstance`). Cost: one virtual call per shader run.

**Alternatives**: dispatch inside `CProgrammableShaderInstance::execute` and make tests call that (rejected: test edits);
a function pointer in the payload (rejected: not a backend interface).

## R5. The renderer-services step (FR-013, FR-015) **(maintainer, P-Q1)**

**Decision (approved P-Q1)**: one MINOR step, `kRendererServicesVersion` 2.3.0 -> **2.4.0**, one history entry, every
addition with a default so the doubles compile unchanged:

1. `MatrixRequest::phase` (`EMatrixPhase::Shade` default, `EMatrixPhase::Bind`): a bind-time request is answered with the
   renderer's raw named-system lookup (R6).
2. `MessageRequest::destBytes` (default 0 = unbounded, today's behaviour): when non-zero and the stored value needs more
   bytes, the implementer answers `TypeMismatch` before writing (any source, `Light` included). Used by
   `op_lightsource_f` with its 64-byte scratch, replacing its direct `getParameterShape()` read (R7).
3. `getShaderName(const ShaderNameRequest &)` (default body: `Unavailable`): the name of the shader bound to the object
   for a source (`Surface` ... `Exterior`), or of a light handle (`Light`). Used by `shaderName(type)` (R7).
4. Wording: `CShadingContext::interpret()` is added and `execute()` becomes the backend dispatcher (the 2.3.0 entry
   recorded changes to the context's public entries in the same history).

**Rationale**: FR-009 (bind-time lookups through `getMatrix`, bit-identical) cannot be met by the 2.3.0 `getMatrix`
(R6); FR-013 asks that `op_lightsource_f` stop reading `getParameterShape()` and that `shaderName(type)` stop reading the
attribute record, and no 2.3.0 member answers either question. MAJOR stays 2 (policy: no MAJOR step until spec 055).

**Alternatives**: (b) no step: Shading special-cases `"current"`/`"object"` at bind time and the implementer tolerates a
null light handle (changes a run-time crashing path, spreads one rule over two domains); the shape check reads Shading's
own instance through the handle and `shaderName(type)` keeps its Graphics State read (Shading -> Graphics State is
allowed), with FR-013/SC-005 amended. Recommended: the step.

**Collision note**: specs 040 and 048 run at the same time. If either also steps the services version, the second to
merge renumbers its entry (textual, no design conflict).

## R6. Bind-time coordinate systems (#134, FR-009) **(maintainer, P-Q2)**

**Finding**: at bind time today `CRenderer::findCoordinateSystem` (`rendererDeclarations.cpp:287`) answers, and it
disagrees with the 2.3.0 `getMatrix` (`rendererShadingServices.cpp:91`) in four cases:

| Name | Bind time today | `getMatrix` as it is |
|---|---|---|
| `"current"` | the current transform (`context->getXform(FALSE)`) | identity |
| `"object"` | the stored identity, silently | `NoObject`; Shading raises `CODE_SYSTEM` "Object system reference without an object" |
| `"light"` | the stored identity | `assert(request.light)`; release dereferences null |
| unknown name | interpreter: `from`/`to` left uninitialised (UB, `PFROMEXPR_PRE` declares them unset); JIT: identity, silent (`op_pfrom` copies) | identity with "Unknown coordinate system" |

`"shader"`, `"camera"`, `"world"`, NDC/raster/screen, the colour names and custom systems agree. (The instance's `xform`
**is** `currentXform` at bind time: the instance is built with it and `init()` runs before anything changes it.)

**Decision (approved P-Q2)**: one Shading resolver for both backends' bind time,
`libshader::bindCoordinateSystem(name, instance, from, to, kind)`, asks `getMatrix` with `phase = Bind` and the instance
as `shader`; the implementer answers bind requests by `CRenderer::findCoordinateSystem` alone (today's answer, exactly).
The interpreter's init reaches it through the `findCoordinateSystem` macro hook; the JIT's init through the resolver
`jitSetInitCoordinateResolver()` already takes (`rslOps.cpp:986`), and `jitSetInitXform` stays for `"shader"`.
**Not found**: both backends take the JIT's present answer, identity and silent (the interpreter's UB gets a defined
value; the JIT is unchanged). Phase 1 characterises every named space under both backends at bind time first.

**Alternatives**: (a) the step above (recommended); (b) R5's alternative (b); for "not found": identity plus the run-time
"Unknown coordinate system" warning (defines the interpreter case differently from the JIT).

## R7. Message passing (FR-013, FR-014) **(maintainer, P-Q3, P-Q4)**

**Decision**:
- `op_lightsource_f` keeps its light (the shading state's `currentLightInstance`, an opaque handle; #115 unchanged) and
  its scratch, and asks `getMessage{Light, ..., destBytes = 64}`; a `TypeMismatch` answer gives today's one warning text
  and result 0. It no longer calls `getParameterShape()`. Output byte-identical (the implementer applies the same size
  rule `op_lightsource_f` applies now).
- `shaderName(type)` asks `getShaderName{source, objectHandle(), light}`; `"lightsource"` passes the iterated light's
  handle (`currentLight->instance`, as today). With no object (the imager) the answer is not found and the result `""`,
  as today. With **no services** the result becomes `""` with one "renderer services do not provide shadername()"
  warning, where today it read the record directly (P-Q4; no approved test calls it without services: grep of
  `case_no_services.cpp`, `test_no_services.cpp`).
- The request-sequence test (FR-014) records, per call, the tuple (builtin, source, object handle, light handle, name,
  expected type) under both backends with the spec 026 double. `dest`, `wantVariable` and `destBytes` describe the
  destination, which differs by design (#121), and are excluded. `lightsource()` is compared only inside an
  `illuminance` loop over one light, where the iterated light and the last light that ran are the same; with several
  lights, or outside a loop (the interpreter answers without a request, F3), the sequences differ by #115, which stays
  open (P-Q3).

**Rationale**: the clarify session keeps #115 and #121 unfixed and outputs byte-identical; a literal "identical requests"
for `lightsource()` would require fixing #115.

**Alternatives**: exclude `lightsource()` from the comparison entirely (weaker); route the shape check through Shading's
own instance (R5 alternative b).

## R8. The file resolver (FR-004)

**Decision**: `IShaderFileResolver` (Shading): `int locate(char *result, const char *name, const char *extension) const`.
The core implements `CShaderSearchPathResolver` (in `searchPathResolver.h`, beside Texturing's) as one line:
`CRenderer::locateFileEx(result, name, extension, path)`, built per request over `currentOptions->shaderPath`.

**Rationale**: keeps every detail of today's search: the dot rule of `locateFileEx` (a name with an extension before
its last separator is used as written for every format, so `parseSloShader` may be handed an `.rslo` file and fail,
which is today's behaviour), `locateFile`'s `CODE_RESOLUTION` info lines, net-file mappings, the temporary directory and
the network fetch. The shader path changes between statements, unlike Texturing's per-frame path, hence per request.

**Alternatives**: a services member (rejected: services are per session, the path is per statement; FR-004 asks for
Texturing's pattern); moving the dot rule into Shading (rejected: no gain, risk of drift).

## R9. Binding and the instance key (FR-008, FR-029) **(order recorded for 057)**

**Decision**: `IShaderBindHost` (Shading, implemented by `CRendererContext`): `uint64_t takeInstanceKey()` and
`void registerLight(CShaderInstance *)` (pushes into `CRenderer::allLights`). `ShadingSystem::bind()` runs today's order
exactly:

1. (core, before the facade) `"null"` returns `NULL`;
2. find or load the description (cache by name first, R10);
3. not found: `CODE_NOSHADER` "Failed to find shader \"%s\"" and `NULL`; wrong type: `CODE_NOSHADER`
   "Shader \"%s\" is not of the expected type" and `NULL` (**no key taken** in either case);
4. `log_info("getShader: loading shader ...")` (moved verbatim);
5. `instance = backend->instantiate(desc, attributes, xform)`;
6. `instance->instanceKey = host.takeInstanceKey()`;
7. light: `instance->createCategories()` (reads the **default** `__category`: parameters are set at step 10), then
   `host.registerLight(instance)`;
8. `arenaBegin(globalArena)`; `backend->initialise(*instance)`; `arenaEnd(globalArena)`;
9. return the instance to the group builder;
10. `instance->setParameters(np, tokens, values)`.

Today steps 5 and 6 are "construct, then take the key"; both orders take the key only after a successful load and type
check, so `shaderOrdinal` advances identically and the `pendingInstanceKey` leak (a `"null"`, missing or wrong-type
light leaves the key set) stays exactly as it is (FR-029; spec 057 owns the fix and its test).

**Alternatives**: the core drives the steps itself (find, take key, instantiate, light branch, initialise): keeps the
light branch textually in `getShader`, but then the group API cannot carry a binding (US4); rejected.

## R10. The cache and its lock (FR-005)

**Decision**: the cache is a `CTrie<CFileResource *>` owned by `ShadingSystem`, consulted by name **before** the format
is resolved, inserted under `desc->name` (the owned copy; the arena-aliasing comment at `rendererFiles.cpp:429` moves
with it), destroyed with the system. One `std::mutex` held across find, load and insert, and never across `initialise`
or any services call that can reach a texture handle. Never `CRenderer::shaderMutex` (gotcha 25).

**Rationale**: today's callers are already serialized (the RIB thread before rendering; `CRenderer::delayedMutex` around
procedural and delayed-archive expansion, `rendererContext.cpp:483-579`), and `rslo.y`'s parser is not reentrant; the
lock costs one uncontended acquire per statement and makes the facade safe for a caller that is not the core (the test
backend). The `.slo` loader calls `getDSO` while loading; `getDSO` takes no lock.

**Alternatives**: no lock, documented precondition (equal today, weaker); a reader-writer lock (no reader exists apart
from the binder).

## R11. The group API (FR-011, FR-012) **(maintainer, P-Q5)**

**Decision**: `ShaderGroupBuilder` (Shading): `begin()`, `shader(request, attributes, xform, host, np, tokens, values)`,
`connect(...)`, `end()`. A group of one layer **is its layer's instance**: `end()` returns the `CShaderInstance *` with
one reference, which the core adopts into the Graphics State slot (`ShaderGroupRef::adopt`) exactly as today, and which
`RiLightSourceV` returns as the `RtLightHandle`. `connect()` and a second `shader()` are refused with `CODE_INCAPABLE`
"Shader group connections and multi-layer groups are not supported before spec 034"; the refused layer is not
instantiated and the group keeps its first layer. Running: `libshader::groupPrepare(const ShaderGroupRef &, ...)` and
`groupExecute(CShadingContext &, const ShaderGroupRef &, float **)` (forward to layer 0's `prepare`/`execute`), used by
`shade()` for surface, displacement, atmosphere, the volume segments and by the imager. The batch is the context's
current shading state, filled from spec 026's `ShaderGlobals` by `shade()` as today.

**Rationale**: pointer identity matters: `RiIlluminate` casts the handle back (`rendererContext.cpp:2457`), the services
`static_cast` a `ShaderHandle` to `CShaderInstance`, and `shaderInstanceOf()` is a `static_cast` from the slot target.
A wrapper object would change all three and every image risk with it. Spec 034 introduces a group object behind the same
`shaderInstanceOf`/`groupExecute` seams.

**Alternatives**: a `CShaderGroup : CShaderGroupRefTarget` wrapper now (rejected for 033: identity changes, no user).

## R12. Loading performance (FR-024) **(maintainer, P-Q8: the threshold)**

**Decision**: two measurements, each on the starting tree (Phase 1) and the finished tree (Phase 9), on an idle machine
through `cpujob`:
1. `tests/benchmark_loading.sh` with `build/src/sloinfo/sloinfo` over all 137 shipped shaders in both formats
   (a driver loop calls the script per file), 5 runs, the first discarded, median reported.
2. In-process: a Phase 1 test program loads the 137 x 2 shipped objects through the loader entry points (before) and
   through `ShadingSystem` (after), 5 runs, median.

"No measurable regression": the finished median is at most the starting median plus the larger of 5% and twice the
starting run-to-run spread. The timing guards of the visual suite run with unchanged `MAX_RATIO`.

**Rationale**: R0.1's figure measures process start-up of one shader and cannot see the facade.

## R13. Boundaries and the architecture data (FR-021, FR-022)

**Decision**: no allowlist line is added or deleted (DB-007, DB-018 and DB-024 have none); `metrics-baseline.txt`
unchanged unless a row falls; new edges are all allowed: Core -> Shading (`shadingSystem.h`, `shaderBackend.h`),
Shading -> Graphics State (`attributes.h`, `xform.h`, `stateHandles.h`), Consumer -> Shading (`sloinfo` ->
`libshader/include/openrender/ShaderMetadata.h`). `init.cpp`'s move takes 20 core references out of a Shading body that
compiled in the core (they were invisible to `archcheck`, #134). DB-007 -> Resolved (033, date); DB-018 rewritten
(measured state; open: group-layer messages and `setmessage` (034), #115, #121); DB-024 narrowed (sloinfo done; oshader
is spec 043's; open: the preview, the data viewer).

## R14. Approved tests (FR-025)

**Decision**: keep the Shading-internal names the approved tests use (R3, R4) and keep `CRenderer::getShader(name, path,
preferredType)` as a core forwarder to the facade (`shader_parameter_fixtures.h:107,112` call it); its body names no
backend. Result: **no approved test needs an edit** for the move. Comments in approved tests that cite old line numbers
(`test_used_parameters_oracle.cpp:18`, `rendered_harness.h:5`) are left as they are. The inventory is in plan.md
(TEST-APPROVAL); it lists the approved tests checked and the new tests.

**Alternatives**: rename and edit the tests (rejected: churn with no gain).

## R15. The metadata API (FR-016 to FR-018) **(maintainer, P-Q7)**

**Decision**: `src/libshader/include/openrender/ShaderMetadata.h` (internal C++, not installed, not in
`OPENRENDER_PUBLIC_HEADERS`) declares `describeShaderFile(const char *nameOrPath, const char *searchPath,
ShaderMetadata &out)` and the engine-free `ShaderMetadata` (format, type name, name, format version, language version,
used-parameters mask when the format records it, JIT-entry presence for `.slo`, parameters: name, type, container/storage
text, array size, output flag, default as typed values and as the stored text, space). Readers per format implement
`IShaderMetadataReader` and live in the new library **`libRSLInfo`** (named by the maintainer 2026-11-14; `src/libshader/metadata/`, Shading), which
also contains the inspection parser that was `libshader_runtime` (R18): the `.rslo` reader wraps `rsloGet`/`rsloDelete`
(unchanged, its header is public), the `.slo` reader wraps `extractMetadataFromFile` (from the internal `libRSLJitMeta`,
formerly `libshader_jitmeta`). `sloinfo` keeps its own format detection by magic,
argument handling and printing, and includes only `ShaderMetadata.h` and common headers.

**Rationale**: byte-identical output needs the inspection parser's data (`space`, `writable`, constant and vertex
containers, `UDefaultVal`), which the loader's `CShader` does not keep; the runtime parser and the loader's parser share
the flex/bison prefix `rslo` and must not meet in one image (`src/libshader/CMakeLists.txt` note), so the readers cannot
be members of backends linked into `libshader_shading` (`libRSLRuntime`): the readers' library and the engine are never linked into one program. The `.rslo` search path (`.`, `OPENRENDERHOME/shaders`,
`SHADERS`), the `.sdr` fallback and its `[INFO]` line stay as `rsloGet` does them. A JIT-less build registers no `.slo`
reader and `sloinfo` keeps its present message.

**Alternatives**: `IShaderBackend::describe()` inside `libshader_shading` (rejected: prefix clash; sloinfo would link the
engine); reimplement the `.rslo` read (rejected: byte-identity risk).

## R16. The FR-003 scan and the SC-005/SC-006 scans

**Decision**: three `cmake -P` checks in the `architecture` label, in the manner of `Geometry_NoHiderShadingNames`
(whole identifiers, code and comments, every `file:line` printed, a fixture run that must fail):
- `Shading_CoreNoBackendNames`: no file under `src/ri/`, `src/display/`, `src/shadeop/`, `src/procedures/`,
  `src/common/` names `parseShader`, `parseSloShader`, `codeArea`, `constantEntries`, `codeEntryPoint`, `initEntryPoint`,
  `jitEntry`, `jitInitEntry`, `TShaderJitEntry`, `CLLVMJitEngine`, `CProgrammableShaderInstance`, `CRsloPayload`,
  `CSloPayload`, `CRslPayload`, `jitSetInitXform`, `jitSetInitCoordinateResolver`, and no file under `src/ri/` names
  `OPENRENDER_HAVE_LLVM`. Known comment hits to rewrite: `rendererFileSearch.cpp:11`, `src/ri/CMakeLists.txt:339`.
- `Shading_JitMessagesThroughServices`: `rslOps.cpp` and `rslBuiltins.cpp` name no `getParameterShape`, `getParameter`,
  `currentAttributes`, `shaderInstanceOf`, `lightInstanceOf`; `shading.cpp`'s `shaderName` bodies name no
  `currentAttributes->`.
- `Sloinfo_MetadataApiOnly`: `src/sloinfo/` includes nothing under `src/libshader/` but
  `libshader/include/openrender/ShaderMetadata.h`, and names no `rsloGet`, `CLLVMJitEngine`, `SLOShaderInfo`.

## R17. Characterisation first (FR-026)

**Decision**: Phase 1, on the unchanged tree, adds and records:
- `ShaderFormat_Precedence` (`tests/shaderformat/`, `cmake -P` driving `orender`): fixture shaders whose `.rslo` and
  `.slo` of one name are compiled from two sources that set different constant colours, so the pixel tells which backend
  ran; every combination of attribute (`slo`/`rslo`/unset/invalid), option (`slo`/`rslo`/unset), file presence
  (both/rslo only/slo only/neither), a dotted name, a `.slo` without metadata (a non-bitcode file named `.slo`), a `.slo`
  calling a missing DSO operand (gotcha 33), and the cache case (one name asked `slo` then `rslo`); records the pixel and
  the stderr diagnostics; runs in `build` and in `build-nojit`.
- `BindSpaces_Characterise` (`tests/shading_contract/`): a light and a surface whose parameter defaults are in
  `"current"`, `"shader"`, `"object"`, `"light"`, `"world"`, `"camera"`, `"NDC"` and a custom system, under both
  backends, bound through the real renderer; records the defaults' floats bit for bit. (An unknown name is not recorded:
  the interpreter's answer is undefined today, R6.)
- `ShadingContract_MessageRequests` (recorded tuples, R7) and `Sloinfo_Output137` (137 x 2 outputs).
- The manifests of the visual suite and the determinism sweep on each platform (method of spec 029, R13 there), the
  spec 026 and 028 replays unchanged, and R12's timings.

**Rationale**: no test pins the precedence today (spec "Measured starting point"); every later phase is checked against
these records.

---

## R18. The libraries' names: `libRSLRuntime` and `libRSLInfo` (maintainer, 2026-11-14)

**Decision**: `libshader_shading` is renamed `libRSLRuntime` and does not absorb `libshader_runtime`; the inspection side
(`libshader_runtime` with its static archive, the metadata API, the `.slo` probe) is **one library, `libRSLInfo`** (the
maintainer's amendment: it replaces the planned `libshader_metadata`); `libshader_jitmeta` becomes the internal
`libRSLJitMeta`; `libshader_compiler` is untouched (spec 043 -> `libRSLCompile`). Full record in plan.md "Library decision
(L-1, L-2)"; the measurements:

| Measurement (macOS `build`, 2026-11-14) | Result |
|---|---|
| `libshader_runtime.1.0.0.dylib` / `libshader_runtime.a` | 72,456 / 37,344 bytes; 45 exported symbols (`nm -gU`); API `rsloGet`/`rsloDelete` (public `rslo.h`) |
| `libshader_shading.1.0.0.dylib` | 43,116,104 bytes; 24,883 exported symbols; contains `rslo.y` (the loader), `sloLoader.cpp`, the JIT and LLVM |
| global symbols defined in both | **37**, all of the flex/bison `rslo` prefix (`rsloparse`, `rslolex`, `rslotext`, `rslolval`, `rsloLineno`, `rsloerror`, `rslo_scan_*`, ...) |
| defined only in `libshader_runtime` | `rsloGet`, `rsloDelete` and six unprefixed globals (`parameters`, `shaderType`, `currentParameter`, `currentDefaultItem`, `numArrayItemsRemaining`, `parsedShaderName`) |
| `sloinfo` | 6,441,784 bytes, links `libshader_runtime`, `libRIBCommon`, the C++ runtime, and statically `libshader_jitmeta` and LLVM |
| consumers of `libshader_runtime` | `sloinfo` (and `rsloinfo`, a symlink); aliases `rslo`/`rslo_static` (`src/rslo/CMakeLists.txt`); exports `openRender::shader_runtime` and `shader_runtime_static` |
| tests naming the target `libshader_shading` | about 45 `target_link_libraries` lines and `$<TARGET_FILE:>`/`$<TARGET_EXISTS:>` uses (alias-safe); **three pin the name in a way an alias does not cover**: `check_scope.cmake:86`, `check_link_line.cmake:56`, `install_consumer/CMakeLists.txt:16` (this one is covered by the exported alias) |
| `libshader_jitmeta.a` | 22,672 bytes; LLVM `core bitreader support` come in at link: they are what makes `sloinfo` 6.4 MB |
| what `libRSLInfo` contains | the 45-symbol parser (once, so no internal clash), the metadata API, and (shared only, JIT on) the `.slo` probe with LLVM: expected about the bytes `sloinfo` carries today, measured again at Phase 8 (T118) |
| `libRSLInfo_static` | parser + `.rslo` half only, no LLVM: the content of `libshader_runtime_static.a` (37,344 bytes) |

**Rationale**: the engine cannot absorb the reader (the 37 clashing symbols; the shared-only engine cannot be a static
export; 43 MB against `sloinfo`'s start-up); the reader, the metadata API and the `.slo` probe belong together (the `.rslo`
reader wraps `rsloGet`, now in one image; `sloinfo` is the one consumer; the one clash is with the engine, so the two
libraries must never be linked into one program: guard `Link_InfoRuntimeNotCoLinked`). Compatibility follows the existing `libRIB*` precedent (`CMake/createCompatAlias.cmake`,
`src/ri/CMakeLists.txt`): an ALIAS target, an exported alias, a symlink for the dev-name file.

**Alternatives**: merge the reader into the engine after renaming the inspection parser's prefix (rejected: grounds above);
leave `libshader_runtime` unrenamed beside `libRSLRuntime` and keep a separate `libshader_metadata` (the tasks-stage
default, rejected by the maintainer: a confusing pair, and a `libshader_*` name left); keep `libshader_shading` as the real target with `libRSLRuntime` an ALIAS (rejected: the
maintainer asked that target and `OUTPUT_NAME` follow the new name).
