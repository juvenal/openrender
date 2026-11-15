# Feature Specification: Shading System Backends

**Feature Branch**: `033-shading-system-backends`
**Created**: 2026-11-13
**Status**: Draft
**Input**: Constitution Principle VIII remediation, layer L5, after spec 029 (merged, with 030, 031, 032 and 056); GitHub #39, plan brief row 033. Shading gets one backend-neutral entry, a `ShadingSystem` facade over `IShaderBackend` implementations (the RSL interpreter and the RSL JIT), and the renderer core stops choosing, loading, binding and initialising shaders by concrete type. Shader loading, the loaded-shader cache and `shaderformat` precedence move into Shading; `CShader` splits into a backend-neutral description plus a per-backend payload; shaders are bound and run through a group-shaped API (a group of one layer until spec 034); the JIT's message-passing lookups reach other domains' data only through the renderer services; a shader-metadata API serves `sloinfo` (and, later, spec 043). Resolves registry entry DB-007, finishes the JIT part of DB-018 and the `sloinfo` part of DB-024. Renders stay byte-identical. The interface stays internal C++ (the public facade is spec 052).

## Clarifications

### Session 2026-11-13 (coordinator, before specify)

- Q: How does 033 relate to spec 057 (area-light geometry), which runs light shaders with the light surface's globals and runs at the same time on its own branch? -> A: **033 merges into master first.** Spec 057 merges master into itself as soon as 033 lands and adapts to 033's facade. 033 therefore designs only against master as it is now, knows nothing of 057's code, and waits for nothing from it. The light-shader path is **moved, not redesigned**: its behaviour stays exactly as today (byte-identical images), and this spec records the entry points a light shader is loaded and executed through after the change (FR-029), so 057 can adapt to them. (coordinator)

### Session 2026-11-13 (maintainer, clarify stage)

- Q: Cache key (FR-005): preserve today's name-only cache or key by name and resolved format? -> A: **Option A, preserve it exactly.** The cache is consulted by name before the format preference, so the first format loaded for a name serves every later request for that name. Recorded as a known limitation (DEVNOTES) with GitHub issue #167.
- Q: How far does the group API go (FR-011)? -> A: **Option A, one layer only.** `connect` is declared and returns an explicit "not supported before spec 034" diagnostic; a group of more than one shader is refused with the same diagnostic. Connection evaluation, lazy layers and cross-layer messages are spec 034's.
- Q: #115 and #121 (FR-014)? -> A: **Option A, fix neither.** 033 reroutes the lookups through the services and keeps both backends' outputs byte-identical to today's, as spec 026 did; both issues stay open and become one-place fixes. This spec's merge does not close either.
- Q: Is #134 (bind-time coordinate-system lookups bypass `getMatrix`) in this spec? -> A: **Yes** (D-spec-1 confirmed); #134 is resolved by this spec; the maintainer closes it at merge.
- Q: Coordination with spec 057 on the light's instance key (coordinator decision) -> A: spec 057 found that `CRendererContext::pendingInstanceKey` leaks: `extractLightHandle` stores it (`rendererContext.cpp` ~:2414 and ~:2440) and only `takeInstanceKey()`, called from `getShader`, clears it, so a `"null"`, missing or wrong-type shader leaves it set. **057 owns that fix and its test.** 033 moves that code exactly as it is (behaviour unchanged, leak included) and MUST NOT fix it (FR-029).
- Note: GitHub #158, #159, #160, #161, #164, #165 and #166 were wrongly closed by a merge-message keyword on 2026-11-13 and have been reopened; they are open defects. Nothing in this spec fixes them, and this spec's merge message must not close them.

### Session 2026-11-14 (maintainer, plan approval)

The maintainer approved every plan decision as recommended; plan.md "Decisions approved" holds the full record.

- Q: P-Q1 (services step)? -> A: **2.3.0 -> 2.4.0**, additive (`MatrixRequest::phase`, `MessageRequest::destBytes`, `getShaderName()`). Spec 057 takes **2.5.0** at its sync (coordinator: 033 merges first).
- Q: P-Q2 to P-Q9? -> A: as recommended. A missing bind-time space gives identity with no warning on both backends (P-Q2); the request-tuple comparison with `lightsource()` compared only in a one-light `illuminance` loop, the rest recorded under #115 (P-Q3); `shadername()` with no services is `""` plus one warning (P-Q4); a one-layer group is its instance (P-Q5); the names approved tests use are kept, so no approved test is edited *for the move* (P-Q6); `libshader_metadata` is a separate library (P-Q7); the timing threshold of research R12 (P-Q8); `lightInstanceOf()` returns `CShaderInstance *` (P-Q9).
- Q: P-Q10 (corrections to this spec)? -> A: applied below: services 2.3.0 (FR-015, A-7), DB-007's line numbers, `OPENRENDER_DEFAULT_FORMAT` as a `#define`, DB-018's remaining scope, and a definition for FR-021's "core's baseline".
- Q: Library names (maintainer, 2026-11-14)? -> A: the shader load and runtime library is renamed **libRSLRuntime** (FR-031). Measured at the tasks stage: `libshader_runtime` is *not* the loader; it is the 72 KB `.rslo` inspection reader (`rsloGet`/`rsloDelete`), while both loaders already live in `libshader_shading`. The two are not merged (plan.md "Library decision": 37 colliding `rslo*` parser symbols, the exported static archive `shader_runtime_static` that the shared-only engine cannot offer, and `sloinfo`'s 72 KB link against a 43 MB engine). `libRSLRuntime` is `libshader_shading` renamed; `libshader_runtime` keeps its name. `libshader_compiler` is not renamed here: spec 043 renames it to `libRSLCompile`.
- Q: 057 coordination? -> A: 033 changes `lightInstanceOf()`'s return type and rewrites `getShader` and binding into `ShadingSystem::bind`, but leaves `RiLightSourceV`, `RiAreaLightSourceV`, `extractLightHandle`, `takeInstanceKey` and `pendingInstanceKey` exactly as they are (leak included; 057 fixes it).

### Decisions recorded at the specify stage

These are placements the specify stage chose, with the reason, so a reviewer can overrule one cheaply. The three open questions (FR-005, FR-011, FR-014) were settled by the maintainer in the clarify session below; no marker remains.

- **D-spec-1**: #134 (the bind-time coordinate-system lookups of `src/ri/render/init.cpp` bypass `getMatrix`) is **in**. Spec 029 left it to this spec (its D-spec-3): the bind-time `init` function is shader binding, so it leaves the core with the rest of binding (FR-009).
- **D-spec-2**: **Group of one layer by default.** The group-shaped API is in, but evaluating connections between layers (parameter connections, lazy layers, `getmessage`/`setmessage` across layers) is spec 034's (its brief). Settled in the clarify session: one layer only (FR-011).
- **D-spec-3**: The plan brief's other 033 items are **in**: `ShadingSystem`, `IShaderBackend`, loading in Shading, the `CShader` split, JIT message passing through services, the metadata API. The public C facade (052), the compile-time metadata publication (043), the OSL backend (049) and native JIT lowering (055) are **out**; this spec only keeps their seams open (FR-030). No interface of this spec names OSL.
- **D-spec-4**: **No decision of `RENDER_ARCHITECTURE_REFERENCES.md` changes.** The spec relies on D25 (the compiler library and, later, a public runtime library over `ShadingSystem`; 026's interfaces stay internal C++), D8 (built-in registries first), D9 (`Surface` stays RSL-only; the `shaderformat` precedence picks between `.slo` and `.rslo`, unchanged), D12/D23/D24 (the JIT-off build and the CMake floor stay until the OSL spec), D22 (the three versions move independently), D27 (native JIT lowering is a later spec) and section 5.4 (`IShadingService` is this `ShadingSystem`). The plan stage reads section 5.4, section 11.1 (adapter steps) and D25.
- **D-spec-5**: **#157** (the shipped `glass` shader renders wrongly under `.slo`) is out; it is a JIT defect in one shader, not a boundary. **#8** is closed. **#115** and **#121** are fixed neither here (FR-014, clarify session).

## User Scenarios & Testing *(mandatory)*

The "users" are the people who build, maintain and extend openRender: contributors to Shading, maintainers reviewing Principle VIII remediation, the authors of the specs that build on this one (034 shader networks, 035 material interface, 043 compiler library, 049 the OSL backend, 052 the public runtime library, 055 native JIT lowering, and 057 for the light path), and anyone who inspects shader files with `sloinfo`.

### Measured starting point (2026-11-13, master `278b0394`)

Method: static reading of the source, the registry and the allowlist in this worktree; `grep` counts. No build was run for the specify stage. Phase 1 re-measures with `archcheck` (including its `CONSUMER` lines) on a fresh macOS build and on Linux. Where the registry text and the code disagree, the code is cited and the registry text is listed for correction (FR-022).

- **The renderer core chooses the backend.** `CRenderer::getShader(name, path, preferredType)` (`src/ri/render/rendererFiles.cpp:372-439`) holds the whole policy: a name cache (`globalFiles`, consulted before anything else), `.slo` first or `.rslo` first by the preference string, `parseShader()` (the `.rslo` loader, `rslo.y:2108`, declared by a bare forward declaration at `rendererFiles.cpp:50`) and `parseSloShader()` (`sloLoader.h`), two `#ifdef OPENRENDER_HAVE_LLVM` branches and the one warning a JIT-less build gives when `slo` is asked for. The preference itself is computed in `CRendererContext::getShader` (`rendererContext.cpp:304-307`): the `Attribute "shade" "shaderformat"` value, else the `Option "shaderformat" "default"` value, else `OPENRENDER_DEFAULT_FORMAT`, which is a literal `#define OPENRENDER_DEFAULT_FORMAT "rslo"` in `config-cmake.h.in:37` (no environment variable or cache entry reads it). The registry's DB-007 text (`rendererFiles.cpp:48,130,693-716`; `shader.h:157,223`) is stale: the code is `rendererFiles.cpp:50` (the bare `parseShader` declaration), `:52` (`sloLoader.h`), `:372-439` (`getShader`), the JIT fields `shader.h:156-159` and `:240-243`, the switch `execute.cpp:502`.
- **`CShader` carries both backends.** One class holds the interpreter's bytecode (`memory`, `codeArea`, `constantEntries`, `varyingSizes`, `strings`, `codeEntryPoint`, `initEntryPoint`) and, under `OPENRENDER_HAVE_LLVM`, the JIT's `jitEntry` and `jitInitEntry` (`shader.h`); `CProgrammableShaderInstance` copies both entries. `friend parseShader` writes its fields. Two tests re-declare `parseShader` themselves (`test_stale_shaders.cpp:45`, `test_format_versions.cpp:61`).
- **The runtime switch is a null test.** `CProgrammableShaderInstance::execute` (`execute.cpp:502`) begins with `if (cInstance->jitEntry != nullptr) { ... return; }` and, for an `.slo`-only shader, `if (code == nullptr) { warning ... }`: the JIT dispatch lives inside the interpreter's function, including the ambient-`Cl` accumulation for light shaders.
- **The core binds and initialises shaders.** `CRendererContext::getShader` (`rendererContext.cpp:298-350`) type-checks, builds the `CProgrammableShaderInstance`, takes the instance key, calls `createCategories()` and pushes a light into `CRenderer::allLights`, then calls `init()`. `src/ri/render/init.cpp` (426 lines) is the shader's init section run in the core: the interpreter's macros instantiated a second time (19 sites per spec 026's record, #134) and the JIT init path (`jitInitEntry`, `jitSetInitXform`, `jitSetInitCoordinateResolver` with `CRenderer::findCoordinateSystem`). It reads `parent->codeArea`/`jitInitEntry` directly.
- **Backend names in the core**: `parseShader`, `parseSloShader`, `OPENRENDER_HAVE_LLVM` (`rendererFiles.cpp`, `init.cpp`), `CShader` bytecode and JIT fields, `CProgrammableShaderInstance` (constructed in `rendererContext.cpp`, taken as a parameter by `init()`), `jitInitEntry`. The core also keeps `CArray<CShaderInstance *> allLights`, `CRenderer::imagerShader`, and `static_cast`s a services request's shader pointer to `CShaderInstance` (`rendererShadingServices.cpp:115-121`): that is the backend-neutral instance base, not a backend type (assumption A-2).
- **Everything else that touches `CShader`/`CShaderInstance`**: Shading (`shading.cpp`, `execute.cpp`, `executeMisc.cpp`, `rslBuiltins.cpp`, `rslOps.cpp`, `shaderPl.*`, `bundles.h`, `RSLShading.cpp`, `serviceTypes.h`), `photon.cpp` and `imager.*` (Hiders and core): about 30 files by `grep`. `RSLShading` (`src/libshader/include/openrender/RSLShading.h`, "Phase A") is an older static facade for the interpreter.
- **JIT message passing is further along than the registry says.** `op_surface_param`, `op_displacement_param`, `op_atmosphere_param`, `op_incident_param`, `op_opposite_param`, `op_attribute_param`, `op_option_param` and `op_rendererinfo_param` (`rslOps.cpp:2219-2272`) call `CShadingContext::jit*Parameter` (`shading.cpp:3930-4000`), which call the same `surfaceParameter`/`attributes`/`options`/`rendererInfo` members as the interpreter, and those ask the services (`queryMessage`, `queryServiceAttribute`, spec 026). What reaches into another domain's data directly is smaller: `op_lightsource_f` (`rslOps.cpp:921`) reads the shading state's `currentLightInstance` and calls `CShaderInstance::getParameterShape`, then asks the services only for the value; `shaderName(type)` (`shading.cpp:2907`) reads the object's attributes directly (null-guarded since 026); and the `lightsource()` result differs between the backends (#115). DB-018's text lists `op_*_param` as open; Phase 1 verifies the measurement above and the registry is corrected (FR-022). What stays open in DB-018 after this spec: the group-layer messages and `setmessage` (spec 034), #115 and #121.
- **`sloinfo`.** `sloinfo.cpp` includes `llvmJit.h` and `sloMetadata.h` (Shading internals) for `.slo`, and `rslo.h` of `libshader_runtime` (the inspection reader, 72 KB; not the loader, which is in `libshader_shading`) (a second, inspection-only `.rslo` parser, `src/libshader/runtime/rslo.y`, beside the loader's `rslo.y`) for `.rslo`. It links `libshader_jitmeta`, a split-off half of `llvmJit.cpp`, because linking `libshader_shading` aborted at start-up while that library had flat-namespace imports (comment in `src/sloinfo/CMakeLists.txt`); spec 029 removed those imports. `oshader.cpp` includes `opcodes.h`, `rslo.h` and `llvmEmitter.h`, all of them `src/libshader/compiler` headers (the compiler is classified as Shading); DB-024's "3 each" is to be re-measured against the `CONSUMER` lines (assumption A-5).
- **Registry and allowlist.** DB-007 (not a dependency edge, "tracked by review"), DB-018 and DB-024 (reported, never failing) have **no allowlist lines**: `grep` of `tests/architecture/allowlist.txt` for the three IDs finds none. The acceptance line about deleting allowlist lines is therefore met with zero deletions, and the check that must carry this spec is a new one (FR-003).
- **Tests that carry the `.slo`/`.rslo` split**: the `-slo` scenes of the visual suite (`tests/visual/CMakeLists.txt`, `OPENRENDER_SLO_VISUAL_TESTS`), the `shading_parity` tests that set `Attribute "shade" "shaderformat"`, `test_format_versions`, `test_stale_shaders`, `tests/shaders/test_sloinfo_output.sh` and its references, and the 137-shader byte checks. **No test pins the precedence and fallback rules themselves**: they are exercised only because a `-slo` scene that fell back to the interpreter would still match its reference (the suite's own comment says so). Phase 1 adds that characterisation on the unchanged tree (FR-026).
- **Cache key.** The cache is consulted by name before the preference is looked at, so the first format loaded for a name serves every later request for it, whatever `shaderformat` says then. This is observable today; see FR-005 (preserved, clarify session).

### User Story 1 - Renders do not change (Priority: P1)

A user renders any scene, interpreter or JIT, any hider, any thread count. After this feature the images, diagnostics and determinism guarantees are the same.

**Why this priority**: The change touches every shader load, bind and execution. A restructuring that changes a pixel is a regression (specs 025 to 029 held the same line).

**Independent Test**: The visual, parity, `shading_parity`, `shading_contract`, libshader, determinism and architecture suites on master and on the finished tree, comparing output bytes on each platform; the `-slo` and `-rslo` scene pairs included.

**Acceptance Scenarios**:

1. **Given** every scene of the visual suite and the determinism sweep, **When** rendered on the finished tree with the same thread count and seed as on master, **Then** each output file is byte-identical to the baseline recorded on the starting tree, `.rslo` and `.slo` alike, on macOS and on Linux against its own baseline.
2. **Given** the trace-request replay of spec 026 and the motion replay of spec 028, **When** they run on the finished tree, **Then** they pass with their recorded files unchanged.
3. **Given** the full suite on each platform, **When** it runs, **Then** its pass/fail set equals the starting tree's.

---

### User Story 2 - The core loads and runs a shader without naming a backend (Priority: P1)

A maintainer reads the renderer core. It asks the shading system for a shader by name, type and format request, receives a backend-neutral handle, and runs and initialises it through that handle. It contains no `.rslo`/`.slo` path, no loader call, no `jitEntry`, no `OPENRENDER_HAVE_LLVM` and no concrete instance class.

**Why this priority**: It is the acceptance line of #39 ("the core references no backend type") and DB-007's Direction.

**Independent Test**: A source scan, in the `architecture` label, of the renderer core and of every domain outside Shading for the backend names listed in FR-003; and a test that registers a test-only third backend with the shading system, loads a fixture shader through it and runs it, with no edit to any file outside Shading.

**Acceptance Scenarios**:

1. **Given** the finished tree, **When** the scan runs, **Then** it finds none of the listed backend names outside Shading and fails if one is added.
2. **Given** a test backend that claims a file kind of its own, **When** a shader of that kind is requested by name, **Then** it is found, loaded, cached, bound and executed through the same entry points as the two RSL backends, and no file outside Shading and the test was edited to make that possible.
3. **Given** a build with `OPENRENDER_ENABLE_JIT=OFF`, **When** a scene asks for `shaderformat "slo"`, **Then** it renders through the interpreter and gives the one warning it gives today.

---

### User Story 3 - Format precedence and fallbacks are exactly today's (Priority: P1)

A scene author chooses `.slo` or `.rslo` per attribute scope, per scene or by the build default. After this feature every choice resolves exactly as before, including every fallback.

**Why this priority**: Format selection moves (the core passes only what the scene said) and no test pins it. A silent change would make `-slo` scenes pass while rendering through the wrong backend.

**Independent Test**: A characterisation test recorded on the unchanged tree and run unchanged on the finished tree (FR-026): for each combination of attribute value, option value and default, with each of the two files present or absent, which backend loaded the shader, and the diagnostics.

**Acceptance Scenarios**:

1. **Given** an attribute value, an option value and the build default that disagree, **When** a shader is requested, **Then** the attribute wins over the option and the option over the default, and the shading system, with no renderer present, gives the same answer from the same three inputs.
2. **Given** `slo` requested and only an `.rslo` on the search path, or the reverse, **When** the shader is requested, **Then** the other format loads silently, as today.
3. **Given** an `.slo` that carries no embedded metadata, or that calls a shadeop operand the session does not provide (gotcha 33), **When** `slo` is requested, **Then** it is refused with the diagnostics of today and the `.rslo` loads.

---

### User Story 4 - A shader is bound and run as a group (Priority: P2)

A contributor binds a shader with a group's begin, shader and end calls and runs the resulting group for a batch. Every binding the renderer makes today (surface, displacement, atmosphere, interior, exterior, light, imager) is such a group of one layer, held in Graphics State's existing slot handle.

**Why this priority**: Spec 034 (shader networks) and the OSL backend (049) are built on groups. Introducing the shape now, with one layer, means they add layers and connections without another change of every call site.

**Independent Test**: Contract tests on the shading system with a test double of the services: build a one-layer group, run it, compare the variables with the same shader run the way the renderer runs it today; ask for a connection and get the "not supported before spec 034" diagnostic.

**Acceptance Scenarios**:

1. **Given** a one-layer group, **When** it runs for a batch, **Then** its outputs equal those of the single instance run by today's code, bit for bit.
2. **Given** an attempt to connect two layers, **When** the connect call is made, **Then** the call returns the explicit "not supported before spec 034" diagnostic, and a group of two shaders is refused the same way.

---

### User Story 5 - JIT message passing goes through the services only (Priority: P2)

A maintainer replacing the renderer (a test double, or another host) provides the services. The shading builtins of the JIT reach the object's attributes, options, bound shaders and light parameters through them, as the interpreter's do.

**Why this priority**: It is DB-018's JIT part, and it is what lets Shading run against a double with either backend.

**Independent Test**: The spec 026 contract double, extended to run the same shaders under both backends, records every request each message builtin makes; the request sequences are equal for the two backends; no message builtin of either backend reads an attribute record, a bound shader or a light instance except through a services answer.

**Acceptance Scenarios**:

1. **Given** a shader that calls each message builtin (`surface`, `displacement`, `atmosphere`, `incident`, `opposite`, `attribute`, `option`, `rendererinfo`, `lightsource`, `shadername`), **When** it runs under both backends against the double, **Then** the recorded requests are identical.
2. **Given** a shading system with no renderer, **When** a JIT shader calls a message builtin, **Then** the answer is "not found" with the one warning per kind that the interpreter gives.

---

### User Story 6 - `sloinfo` reads shaders through a Shading-owned metadata API (Priority: P2)

A user runs `sloinfo` (or `rsloinfo`) on a `.slo` or `.rslo` file. The output is the same as before; the tool includes no Shading internal and parses nothing itself.

**Why this priority**: DB-024's `sloinfo` part, and the base on which spec 043 later publishes compile-time metadata.

**Independent Test**: `tests/shaders/test_sloinfo_output.sh` and its references run unchanged; a scan shows `sloinfo` includes only the metadata API header and common headers; a unit test asks the API for the metadata of each shipped shader in both formats without loading or running anything.

**Acceptance Scenarios**:

1. **Given** the 137 shipped shaders in both formats, **When** `sloinfo` runs on each, **Then** its output is byte-identical to the starting tree's.
2. **Given** a file that is neither format, **When** the API is asked, **Then** it reports "not a shader object" with the diagnostics `sloinfo` gives today.

---

### Edge Cases

- **A JIT-less build.** `OPENRENDER_ENABLE_JIT=OFF` registers one backend; a request for `slo` warns once and falls back, as today. `sloinfo --slo` in such a build keeps its present message.
- **Loading under concurrency.** The loaded-shader cache is touched at bind time, from the RIB parser's thread; the rule that nothing holding the core's `shaderMutex` may reach a texture handle (gotcha 25) is unchanged. The plan decides the cache's own lock.
- **Search-path changes between statements.** The shader search path is an option that can change between statements; the cache is by name today (FR-005, preserved), so a later request for a cached name never searches again. Kept as is (FR-005, preserved).
- **A shader that fails to load** reports `Failed to find shader "..."` with the same code and text, and the binding statement is ignored as today; a type mismatch reports `is not of the expected type`.
- **The name `"null"`** unbinds a shader: handled before the shading system is asked, as today.
- **A surface's JIT running a light's JIT** through `illuminate()` (the nested save and restore of the active context in the JIT dispatch, `execute.cpp:511-513`): the move keeps the save and restore.
- **Imager shaders** run outside a shading context's object; they go through the same group handle, and `activeContext` handling for them is unchanged.
- **A shader instance's lifetime** follows its reference count in Graphics State's slots; moving creation into Shading changes who allocates it, not who frees it.
- **Library build products.** `libshader_shading` is renamed `libRSLRuntime` (FR-031); `libshader_runtime` (the inspection reader) and `libshader_jitmeta` stay; no installed header changes, and `RSLShading` (a public static facade, `shade()` of a prepared instance) keeps working or is retired in the plan with the same effect on its callers (assumption A-6).

## Requirements *(mandatory)*

### Functional Requirements

**The facade and its backends**

- **FR-001**: Shading MUST own one `ShadingSystem`: the only operations the renderer core uses to find, load, cache, bind (instantiate) and initialise a shader, to ask for its description and to run it. It MUST work with no renderer present (a test double of the services and of the file resolver is enough).
- **FR-002**: Shading MUST define a backend interface (`IShaderBackend`) and register two built-in implementations, the RSL interpreter (`.rslo`) and the RSL JIT (`.slo`). A backend owns what is specific to its format: the file kind it claims, loading a file into a description plus payload, the payload's lifetime, the instance's execution, the bind-time initialisation, and its metadata. Backends MUST be registered through a registration hook, not by a chain of comparisons in the facade, so a third one (spec 049) is added without editing the facade or the core.
- **FR-003**: The renderer core and every domain outside Shading MUST reference no backend type or backend switch: no loader function (`parseShader`, `parseSloShader`), no bytecode or JIT field of a shader (`codeArea`, `constantEntries`, `codeEntryPoint`, `initEntryPoint`, `jitEntry`, `jitInitEntry`), no JIT engine type, no `CProgrammableShaderInstance`, and no `OPENRENDER_HAVE_LLVM` guard in `src/ri`. A source scan in the `architecture` label MUST enforce the list (in the manner of `Geometry_NoHiderShadingNames`) and MUST fail on a new occurrence. The list's exact identifiers are fixed at plan.
- **FR-004**: Locating a shader file MUST go through a file-resolver interface that Shading owns and the renderer core implements over the shader search path (the way Texturing's resolver works, spec 027). Shading MUST NOT call the renderer core to find a file.
- **FR-005**: The loaded-shader cache MUST live in the shading system, with the lifetime the cache has today (created and destroyed with the session's shader state, and surviving frames). Its key behaviour is preserved exactly (clarify session): the cache is consulted by name before the format preference, so the first format loaded for a name serves every later request for that name, whatever `Attribute "shade" "shaderformat"` says by then (a scene asking for `slo` on one object and `rslo` on another for the same shader gets the first one's format for both). This is a **known limitation**: DEVNOTES records it, with GitHub #167; the FR-026 characterisation test pins it, so a later fix is a deliberate, visible change. Images stay byte-identical by construction.
- **FR-006**: The `shaderformat` precedence MUST be evaluated in Shading. The core passes the scene's values (the attribute's, the option's, or none) and Shading applies: attribute, else option, else the build default `OPENRENDER_DEFAULT_FORMAT`; then `.slo` first or `.rslo` first, with every fallback of today and the same diagnostics: the single warning when `slo` is asked of a JIT-less build, the silent fallback when a file is absent, the refusal of an `.slo` without metadata or with an unprovided shadeop operand (gotcha 33). The RIB-level validation of an `Option`/`Attribute "shaderformat"` value (`"slo"`, `"rslo"`, `"default"`) stays where the statements are parsed.

**Description, payload and instance**

- **FR-007**: `CShader` MUST split into a backend-neutral description (name, type, parameter list with their stored types and defaults, used-parameters mask, flags, per-shader data) and a payload that only its backend sees (the interpreter's bytecode and tables; the JIT's module and entries). Everything outside Shading, and Graphics State's slots, read only the description.
- **FR-008**: A shader instance MUST be created by the shading system for a description, an attribute record and a transform, and be reachable by the core and by Graphics State only through the backend-neutral instance interface and `ShaderGroupRef` (assumption A-2). The instance key (spec 039), the light category computation and a light's registration among the frame's lights stay as today (FR-029).
- **FR-009**: The shader's bind-time initialisation (`init`) MUST run inside the backend, and `src/ri/render/init.cpp` MUST leave the core. Coordinate systems resolved at bind time MUST go through the services' `getMatrix` like every lookup made while a shader runs, so Shading's macro sites exist once (#134). Parameter defaults in named spaces MUST come out bit-identical to today's (the GitHub #123 behaviour).
- **FR-010**: Running an instance MUST dispatch by backend through the backend interface, not by testing a field for null inside the interpreter's function. The interpreter's `execute` MUST contain no JIT code. Everything the JIT dispatch does around the call today (saving and restoring the active context around a nested light, the ambient `Cl` accumulation of a light shader, the "no bytecode and no JIT entry" warning) MUST be kept with the same effect.

**The group-shaped API**

- **FR-011**: Shading MUST offer the group-shaped API: begin a group, add a shader to it, connect shaders, end the group, run a group for a batch. Every binding the renderer makes today MUST be a group of one layer, held in the slot handle Graphics State already has. Running a group for a batch MUST give the variables that running the single instance gives today. How the batch is described to a group (the spec 026 `ShaderGlobals` and the context's state) is fixed at plan. The group API has **one layer only** (clarify session): the connect call is declared and returns an explicit "not supported before spec 034" diagnostic, and a group with more than one shader is refused with the same diagnostic. Connection evaluation, lazy layers and `getmessage`/`setmessage` across layers are spec 034's.
- **FR-012**: The group API MUST NOT require a RIB statement or change any RIB grammar; no `Pattern`, `Bxdf` or `reference` syntax arrives here.

**Message passing**

- **FR-013**: Every message-passing builtin of the JIT (`surface`, `displacement`, `atmosphere`, `incident`, `opposite`, `attribute`, `option`, `rendererinfo`, `lightsource`, `shadername`) MUST reach the attribute record, the bound shaders and the lights only through the renderer services, with no read of a record, shader or light instance of another domain by a JIT entry point. This includes `op_lightsource_f`'s access to the current light instance and to `getParameterShape`, and `shaderName(type)`'s direct read.
- **FR-014**: The two backends MUST make the same service requests for the same shader, the same object and the same state, and a test MUST compare the recorded sequences (in the manner of spec 026's trace-request replay). What each backend does with the answer stays exactly as today: **#115** (the JIT's `lightsource()` reads the last light that ran, floats only, and returns the value, where the interpreter reads the iterated light, all types, and returns the found flag) and **#121** (a float destination of unknown size is overflowed by a multi-float value; fixing it needs the destination length from the compiler, an ABI change) are **fixed neither here** (clarify session). 033 reroutes the lookups and keeps both outputs byte-identical; both issues stay open and become one-place fixes. No compiler ABI step.
- **FR-015**: The services interface (`kRendererServicesVersion`, now **2.3.0**: 2.2.0 spec 032, 2.3.0 spec 031) MUST change only if the plan finds it must. The plan found it must (bind-time lookups, `op_lightsource_f`, `shadername()`): spec 033 steps it to **2.4.0**, additive with defaults (`MatrixRequest::phase`, `MessageRequest::destBytes`, `getShaderName()`), with a history entry; MAJOR stays 2 until spec 055. Spec 057 takes **2.5.0** at its sync. Every implementer keeps its MAJOR 2 `static_assert`.

**The metadata API**

- **FR-016**: Shading MUST offer a metadata API that, given a file path, answers without loading or running anything: whether the file is a shader object and of which format, the shader's name and type, its format and language versions, the used-parameters mask, and each parameter's name, type, storage class, array size, output flag and default, and each local the tool prints today. It MUST answer for both formats through the same call; each backend supplies its own half.
- **FR-017**: `sloinfo` MUST use only that API: it includes no Shading internal header and parses no shader format itself, and its output for every shipped shader in both formats is byte-identical. The API MUST be usable from a program that links no renderer, so the tool needs no renderer symbol (the build arrangement, whether `libshader_jitmeta` is kept, is the plan's).
- **FR-018**: The metadata API MUST be the one spec 043 builds the compile-time metadata on: it MUST describe parameters and types in terms that do not name an engine (no bytecode index, no LLVM type). `oshader`'s own include of Shading internals MUST be removed where a remaining one exists after the re-measure (assumption A-5).

**Boundaries, builds and discipline**

- **FR-019**: Images, diagnostics (code and text) and determinism MUST be unchanged: byte-identical output at every thread count on macOS and Linux, for `.rslo` and `.slo`, in every hider.
- **FR-020**: A JIT-less configuration (`OPENRENDER_ENABLE_JIT=OFF`) MUST build, link and run as today, with one backend registered.
- **FR-021**: No new cross-domain include or link edge outside constitution Principle VIII's table. Shading depends on Graphics State, Texturing and Shader DSO Operands only, so every need that now sits in the core's loader (file search, the shader-format values, the instance key) arrives through an interface Shading owns. `Architecture_DomainBoundaries` MUST pass; the allowlist MUST gain no line; the `CRenderer::`/`CRendererContext::` metric for Shading stays 0 and no class row of `tests/architecture/metrics-baseline.txt` rises (that file has no row for the core itself: the metric counts other classes' references to the core, so "the core's baseline only falls" means every class row stays at or below its recorded value; this spec moves no reference into a domain).
- **FR-022**: Registry: DB-007 MUST move to Resolved with spec 033 and the date. DB-018 MUST be rewritten with the measured state (what the JIT's message passing did and does, the `op_*_param` text corrected) and stay Open for what remains (group-layer messages and `setmessage` from spec 034, and #121 and #115, which this spec fixes neither). DB-024 MUST be narrowed by the `sloinfo` part and the re-measured `oshader` part, and stays Open for the rest (the preview and the data viewer). No allowlist line is deleted because none exists for these IDs; the `CONSUMER` figure and the baselines are updated. Nothing recorded refers to the merge.
- **FR-023**: The shading system, the backend interface and the metadata API are **internal C++**: not installed, not in `OPENRENDER_PUBLIC_HEADERS`, no plug-in ABI and no semantic version of their own. The public C facade is spec 052 (D25).
- **FR-024**: Performance MUST not regress measurably: the existing timing guards of the visual suite (the `.slo`/`.rslo` ratio tests) pass with unchanged thresholds, and `tests/benchmark_loading.sh` on the shipped shader set shows no measurable regression in load time (the figure and method are fixed at plan, from a measurement on the starting tree). No thresholds are widened.
- **FR-025**: Tests that depend on the moved code (the two that re-declare `parseShader`, the ones that include `shader.h` for the JIT fields, the contract harnesses) MUST be adapted to the new interface in the same change; none is disabled and none has its tolerance widened.
- **FR-026**: Before any code moves, Phase 1 MUST add and record, on the unchanged tree, a characterisation test of the precedence, fallback, cache and warning behaviour (US3), and the baselines of every suite named in US1 and the loading benchmark; the same tests run unchanged at the end.
- **FR-027**: Documentation MUST be updated with the change: `DEVNOTES.md` (status), `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` (FR-022), `DEVNOTES_DETAILS/OSHADER_UPDATES.md` where the loader or the `.slo` format paragraphs change, and a `claude-md-draft.md` in the spec directory with the revised wording of the gotchas this spec touches (3, 23, 25, 27, 28, 33, 35 and the "Shading pipeline" and "Build" notes of `CLAUDE.md`) and any new gotchas, for the maintainer to apply.
- **FR-028**: No task, requirement or document of this spec depends on the merge (no post-merge step, no merge-commit hash). Registry entries move to Resolved with the spec number and the date; issues are closed by the maintainer at merge through `Closes #NN` lines the maintainer writes (this spec resolves #39 and #134, and not #115, #121 or the reopened #158, #159, #160, #161, #164, #165, #166).

**Light shaders: load and run entry points (for spec 057)**

- **FR-029**: This spec MUST keep light-shader behaviour exactly as today (byte-identical images; a move, not a redesign) and MUST record, in the spec directory (`contracts/light-shader-entry-points.md`, written by the plan stage), the entry points a light shader goes through after the change, each next to its present equivalent, so that spec 057, which merges master after this spec and adapts, knows where to hook. The record covers at least: (1) the call that loads and binds a `LightSource`/`AreaLightSource` shader (today `CRendererContext::getShader(name, SL_LIGHTSOURCE, ...)`, whose callers `RiLightSourceV`/`RiAreaLightSourceV` are not changed by this spec); (2) where `createCategories()` and the registration into the frame's light list (`CRenderer::allLights`) happen; (3) the light's bind-time init; (4) the call a surface's `illuminance`/`illuminate` makes to run a bound light (today `CShaderInstance::illuminate` and the active-light entry `lightInstanceOf`), and how its output (`Cl`, `L`, the saved ambient state, `__nonspecular`/`__nondiffuse`) reaches the caller; (5) the JIT `lightsource()` lookup path; (6) the context save around a nested light's JIT call. **Known leak, moved not fixed**: the light's `pendingInstanceKey` (stored by `extractLightHandle`, `rendererContext.cpp` ~:2414/:2440; cleared only by `takeInstanceKey()` from `getShader`) stays set after a `"null"`, missing or wrong-type light shader. 033 moves that code exactly as it is, leak included, and MUST NOT fix it; the entry-point record states this next to entry point (1). Spec 057 owns the fix and its test. 033 designs against master as it is; it reads no 057 code and waits for none.
- **FR-031**: The shader load and runtime library MUST be named **libRSLRuntime** (maintainer, 2026-11-14): the CMake target (`libRSLRuntime`, the `lib`-prefixed family of `libshader_compiler`, which spec 043 renames `libRSLCompile`), `OUTPUT_NAME RSLRuntime`, `EXPORT_NAME RSLRuntime` (`openRender::RSLRuntime`) and the installed files (`libRSLRuntime.{dylib,so}`) follow the new name; it is today's `libshader_shading` (the shading engine with both shader loaders, the JIT and the `ShadingSystem`). `libshader_runtime` (the `.rslo` inspection reader, public header `rslo.h`), `libshader_jitmeta`, `libshader_metadata` and the static `shader_runtime_static` keep their names. The old name stays for one release as an ALIAS target (`libshader_shading`), as an exported alias (`openRender::shader_shading`) and as a compatibility symlink next to the installed file (the `createCompatAlias.cmake` mechanism of `libRIB*`). The compiler library (`libshader_compiler`) is NOT renamed here: spec 043 renames it `libRSLCompile`. Tests that pin a library or install name change only with the maintainer's approval (plan.md TEST-APPROVAL). Every document the rename touches is updated.
- **FR-030**: Seams for later specs MUST stay open without being built: a group can later hold several layers (034); a backend can later produce a closure tree beside colour (035; no closure type is named here); a backend can later be an OSL one (049), whose payload and metadata sit behind the same two interfaces; compile-time metadata can be published from the metadata API (043); the interfaces can be wrapped by a public library (052); the JIT backend can later change its lowering without a change to the facade (055). A short note per spec in the plan records the seam; none is implemented.

### Key Entities

- **ShadingSystem**: the single Shading entry the renderer core uses for finding, loading, caching, instantiating, initialising and running shaders, and for asking what a shader is. Owns the loaded-shader cache and the registered backends.
- **IShaderBackend**: what a shader format must provide (claim a file kind, load, create a payload, instantiate, initialise at bind time, execute, describe). Two built-in implementations; a test backend; the OSL one later (049).
- **Shader description**: the backend-neutral view of a loaded shader: name, type, parameters and defaults, used-parameters mask, flags, per-shader data. The part of today's `CShader` the rest of the program may read.
- **Shader payload**: the backend-private part: the interpreter's bytecode and tables, the JIT's compiled module and entry points.
- **Shader instance / group**: a shader bound with parameter values, an attribute record and a transform; a group is a set of layers, one for now, and is what Graphics State's slot handle (`ShaderGroupRef`) refers to.
- **Format request**: the scene's `shaderformat` values (attribute, option) that the core passes unevaluated, resolved by Shading with the build default.
- **File resolver**: the Shading-owned interface through which the core's shader search path is reached.
- **Shader metadata**: the engine-free description of a shader file (format, name, type, versions, used parameters, parameters and locals), answered without loading or running it; read by `sloinfo` and later by the compiler library.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every image of the visual suite and of the determinism sweep (582 entries) is byte-identical to the baseline taken on the starting tree, `.rslo` and `.slo`, on macOS and on Linux; the spec 026 and 028 replays pass with their recorded files unchanged; the pass/fail set of the full suite equals the starting tree's.
- **SC-002**: The source scan of FR-003 finds 0 backend names in the renderer core and in every domain outside Shading, and has a mutant check (an added name makes it fail); `OPENRENDER_HAVE_LLVM` appears 0 times under `src/ri` (it appears in 2 core files today).
- **SC-003**: A test-only third backend is registered, found by name, loaded, cached, bound, initialised and run with 0 edits to any file outside Shading and the test.
- **SC-004**: The precedence characterisation (every combination of attribute, option, default and file presence) gives the same result on the finished tree as on the starting tree, and the shading system gives that result from the three inputs with no renderer present; the JIT-less configuration builds, and gives the same warning and the same images.
- **SC-005**: The recorded service requests of every message builtin are equal under both backends; 0 direct reads of an attribute record, a bound shader or a light instance remain in a JIT entry point (checked by a scan, list fixed at plan).
- **SC-006**: `sloinfo` includes 0 Shading-internal headers and 0 format parsers of its own; its output for all 137 shipped shaders in both formats is byte-identical, and `tests/shaders/test_sloinfo_output.sh` passes unchanged.
- **SC-007**: `src/ri/render/init.cpp` is gone from the core; the 19 macro re-instantiations of #134 are 0; bind-time lookups go through `getMatrix`.
- **SC-008**: `ctest -L architecture` passes with an allowlist that gained 0 lines; DB-007 is under Resolved; DB-018 and DB-024 are rewritten with the measured state; the baselines only fell.
- **SC-009**: The timing guards of the visual suite and the loading benchmark show no regression, with 0 thresholds widened and 0 tests disabled.
- **SC-010**: The record of the light-shader entry points (FR-029) exists and names its six entry points with their present equivalents, and the light-shader scenes of the suite (`-slo` and `-rslo`, including the area-light ones) are byte-identical.
- **SC-011**: `libRSLRuntime` is built and installed under its name; the old target, exported and file names resolve for one release; a package consumer (`find_package(openRender)`) reaches both `openRender::RSLRuntime` and `openRender::shader_shading` (`ctest -L '^link-package$'` and `PluginAbi_InstallLayouts`); `tests/link/check_scope.cmake` still reads the renamed target's include path (shown failing on a planted include); no other approved test is edited without approval.

## Assumptions

- **A-1**: Dependencies are merged: spec 029 (and 030, 031, 032, 056) are ancestors of master `278b0394` (checked with `git merge-base --is-ancestor`); `hcshader` is gone, so `CProgrammableShaderInstance` is the only class derived from `CShaderInstance`.
- **A-2**: The backend-neutral instance base (`CShaderInstance`, or whatever the plan names its successor) is not a "backend type" for FR-003; `CProgrammableShaderInstance`, the payload and the JIT types are. Whether the abstract base stays a class or becomes the group handle's interface is the plan's choice, as long as the core names no concrete type.
- **A-3**: Spec 033 designs only against master as it is. Three other specs run at the same time on other branches (040 GI determinism, 048 OIIO image I/O, 057 area-light geometry); their unmerged code is not read and nothing here is shaped around it. The merge order with 057 is the coordinator's decision recorded in Clarifications.
- **A-4**: The preference values, the validation of `Option`/`Attribute "shaderformat"`, `CAttributes::shaderFormat` and `COptions::defaultShaderFormat` stay in Graphics State and the Scene Interface as strings; only the evaluation of precedence and everything after it moves.
- **A-5**: `oshader`'s Shading-internal includes in DB-024 are, on reading, its own compiler's headers (`opcodes.h`, `rslo.h`, `llvmEmitter.h`); whether the registry's figure counts them as a violation is settled by Phase 1's re-measure. What belongs to this spec for `oshader` is whatever the re-measure shows reaching the engine (not the compiler); the compiler library (spec 043) owns the rest.
- **A-6**: `RSLShading` (the Phase-A public static facade) is not extended; the plan decides whether it forwards to the shading system or stays, with no change in what its callers observe. No installed header changes in this spec.
- **A-7**: The services interface is at 2.3.0 on master; the plan adds members, so FR-015 applies and the step is 2.4.0 (approved).
- **A-8**: Linux validation follows the per-spec tree and remote `cpujob -j2` arrangement of earlier specs; GCC 13 `-Werror` applies.
- **A-9**: The `.slo` format version (2.0.0, or 2.1.0 for a module calling a DSO operand) and the `.rslo` format are untouched.

## Concurrent work and merge-order risks

Three other specs run at the same time on their own branches. By the coordinator's decision (Clarifications), **033 merges into master first**, and the other branches merge master and adapt.

- **057 area-light geometry (the one real overlap).** It makes `AreaLightSource` bind its scope's geometry as the emitter and runs a light shader once per sample with the light surface's globals; it touches Shading (the light iteration and the light's globals) and the way a light shader is loaded and run. 033 moves exactly the code around that path: `CRendererContext::getShader` and its light branch (`createCategories`, the light list), the light's bind-time `init`, `CProgrammableShaderInstance::execute`/`illuminate` (with the JIT dispatch, the ambient-`Cl` accumulation and the nested-light context save), `lightInstanceOf`, and the JIT `lightsource()` lookup. Likely conflict files: `rendererContext.cpp` (`getShader`, `RiLightSourceV`/`RiAreaLightSourceV`), `shading.cpp` (`iterateLights`, the light's globals), `execute.cpp`, `shader.h`/`shader.cpp`, `rslOps.cpp`. Mitigation, decided: 033 keeps light behaviour exactly as today and records its entry points (FR-029); 057 merges master as soon as 033 lands and adapts. If 057 reached master first anyway, 033 would have to adapt, and the maintainer should decide the order again.
- **040 (GI determinism)** and **048 (OIIO image I/O)** do not touch shader loading. Both can touch the shared documents and lists that every spec edits: `DEVNOTES.md`, `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`, `tests/architecture/allowlist.txt` and `metrics-baseline.txt`, `CLAUDE.md` (through each spec's draft), the root and `src/*/CMakeLists.txt` lists, and `vcpkg.json` (048). Expect textual merge conflicts there, not design ones; whichever merges second rebases and recounts the baselines.
