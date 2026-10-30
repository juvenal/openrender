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
- Q: Are the 56 existing `CRendererServices` methods regrouped? -> A: No. The v2 services are added as new members; the plan classifies each existing method as unchanged, replaced or removed (FR-014). (maintainer)
- Q: What does a failed `get_attribute`/`getmessage` query return? -> A: An explicit status (found, not found, type mismatch); the conversion and refusal logic stays in Shading, applied to the service's raw typed value. (maintainer)

## User Scenarios & Testing *(mandatory)*

The "users" of this feature are the people who build, maintain and extend openRender: contributors working in the Shading domain, maintainers reviewing remediation specs, the authors of the specs that build on this boundary (029 hider/shading separation, 033 shading-system backends, 034 shader networks, 049 the OSL backend, 052 the public runtime library), and anyone rendering scenes that use message passing, attribute and option queries, coordinate-system transforms and traced lookups.

### Measured starting point (2026-10-30, master `d01da47`)

Method: static counts from the source and the allowlist of this worktree (no build was run for the specify stage). The plan re-measures them with `archcheck` on a fresh macOS build, and the first implementation tasks re-take them once more after any rebase (spec 027's practice).

- **The services interface is one growing class with one real implementer.** `CRendererServices` (`src/libshader/shading/rendererServices.h`) has 56 virtual methods: frame and option getters, resource getters, coordinate-system lookup, filter/DSO/variable lookup, the 3D-texture channel forwarders (spec 027) and `textureSystem()` (spec 027). `CRendererServicesImpl` (renderer core) is its only production implementer; three test classes derive from it (`CRecordingServices`, `CForwardingServices`, `CStubServices`), and they **forward to a real inner implementation**, so none is a standalone double. It carries no version.
- **Message passing and attribute reads bypass the services.** `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()`, `attribute()` and the shader-name query read `currentShadingState->currentObject->attributes` themselves (`shading.cpp`, the `*Parameter` members and `attributes()`), resolving the bound shader through spec 025's handle accessors. `lightsource()` reads the current light entry. `option()` already goes through the services (`lookupUserOption` and the option getters). Further reads of the same field sit in `execute.cpp` (2 sites), `executeMisc.cpp` (2 sites), `shaderFunctions.h` and `giFunctions.h` (flags, shading rate, photon estimator). The JIT's `op_*_param` entry points call the **same** `CShadingContext` members as the interpreter, so a change inside those members reaches both backends.
- **Coordinate systems are half routed.** Named systems come from `findCoordinateSystemWithType` on the services; the object, shader and light systems are resolved inside Shading from `currentObject->xform`, `currentShaderInstance->xform` and `currentLightInstance->xform` (`shading.cpp:2855-2910`). About 30 call sites reach it (31 mentions in the shading sources, declarations and comments included). No call carries a time.
- **Tracing is a member of the shading context whose body lives in Hiders.** `trace(CRay *)` and `traceEx(CRayBundle *)` are declared in `shading.h` and defined in `src/ri/hiders/trace.cpp`; `executeMisc.cpp` (transmission, visibility, trace) and `shading.cpp` (gather) create the hiders' bundle classes (`CTraceBundle`, `CTransmissionBundle`, `CGatherBundle`, declared in `src/ri/hiders/bundles.h`) and call them. These account for 6 of DB-001's 8 allowlisted link edges (`trace`, `traceEx` twice, the two bundle vtables, `CGatherBundle`'s constructor; the other 2 are `CPhotonMap::lookup`); the include side is DB-005's 8 edges. The bundle vtables are **data** imports, which dyld binds eagerly, so `libshader_shading` cannot even load without a definer for them.
- **Ray type and hit mode are inferred, not passed.** `shade()` receives a shading dimension (`SHADING_0D`, `SHADING_2D_GRID`, `SHADING_2D`) and the hit mode is the object's `camerahitmode` attribute, read only when the dimension is a grid (`shading.cpp:1374`); the photon pass is told apart by the illumination hook (spec 024); a ray carries `flags`, `depth` and a sample key (spec 039). There is no single value that says "this is a camera hit", "a shadow ray", "a photon" or "a grid".
- **Shading's coupling to Geometry (DB-006) has 3 Shading-origin include edges**: `execute.cpp` and `executeMisc.cpp` to `surface.h`, `shading.cpp` to `object.h`. DB-006's allowlist holds 33 lines in all (26 include, 7 link). `shade(CSurface *, ...)` pulls its inputs from the primitive; switching that to a filled-in structure is spec 029's.
- **Tests that exercise the services link the renderer.** Of the 23 test programs in `tests/shading_parity`, 19 link `ribRender` together with `libshader_shading`. No test runs Shading against a standalone double of the services.
- **Texture access is already routed** (spec 027): `CRendererServices::textureSystem()` and `libshader/shading/textureCalls`; `tests/shading_parity/recording_texture_system.h` is a recording double of the texture system.

### User Story 1 - Shading has one documented, versioned, test-doubled boundary (Priority: P1)

A developer, or a maintainer reviewing a Principle VIII remediation, runs the shading library against a standalone double of its services, with no renderer in the process. Today the services interface is a flat list of 56 methods, only forwarding wrappers of the real implementation exist, and every test that exercises the services links the renderer. After this feature the boundary is a documented v2 interface with a version constant, and a standalone double implements it.

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
3. **Given** a shader that queries a name that does not exist (an unknown attribute, a shader of a type that is not bound, a missing coordinate system), **When** it runs, **Then** the result and the diagnostic are the same as today.

---

### User Story 3 - Message passing and attribute queries go through the services (Priority: P1)

A shader author uses `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()`, `lightsource()`, `attribute()` and `option()`. A developer who embeds the shading library, or who writes the OSL backend, supplies those answers from their own data. Today Shading reaches into the attributes of the current object to answer most of them. After this feature each answer comes from a service call, the same one from both backends.

**Why this priority**: It is DB-018 (V15 step 1), the largest piece of the remediation, and it is what lets Shading stop depending on the layout of Graphics State for per-point data.

**Independent Test**: Install a double whose attribute and message tables are canned; run one shader per builtin under `.rslo` and under `.slo`; compare what the double was asked and what the shader got with the answers the real renderer gives on the same scene.

**Acceptance Scenarios**:

1. **Given** a double with canned answers, **When** an interpreted shader and the same shader compiled for the JIT call each of the builtins above, **Then** the double receives the same request (point, source or object, name, expected type) from both backends and the builtin returns the double's answer.
2. **Given** the real renderer, **When** the same shaders run on the finished tree, **Then** every returned value, including the found/not-found result and the type-mismatch behaviour, equals master's.
3. **Given** the shading sources, **When** they are inspected, **Then** none of the builtins named in this story reads a field of the current object's attributes; the reads that remain (for example the per-object flags the executor tests) are listed in the plan with the reason each stays.

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
2. **Given** each situation in which the renderer shades today, **When** the mapping table is read, **Then** it names the ray type and hit mode the situation has, derived from the code (grid versus ray dimension, `camerahitmode`, the illumination hook, the photon hider), and a test reproduces the derivation for every situation the visual suite exercises.
3. **Given** a field a producer cannot fill (a ray hit has no grid differences; a photon has no screen-space derivatives), **When** the structure is built, **Then** the contract says how absence is expressed and consumers are required to check it, rather than reading garbage.

---

### User Story 6 - Tracing goes through a Shading-owned interface (Priority: P2)

A developer testing a shader that traces, or writing a hider, supplies the answer to "trace these rays" through an interface Shading owns. Today the call is a member of the shading context whose body is in Hiders. After this feature `ITracer` is that interface; the existing hiders' code answers it unchanged, and a double answers it in tests.

**Why this priority**: V2(d), the upward capability that Principle VIII rule 2 requires to be inverted. P2 because hiders owning a context and implementing the interface is spec 029's, so this story defines and adapts, and does not remove the link edges.

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
- **Name lookups that fail**: an attribute, option, message source or coordinate system that does not exist returns not-found and leaves the destination untouched, exactly as today; a type mismatch between what the shader asks and what exists keeps today's conversion or refusal.
- **Varying versus uniform queries**: message passing is evaluated once per call with the first name (`name[0]`) and copied to every active lane; the services see one request per call, not one per lane. Active-lane masks and the uniform-conditional behaviour of the JIT (GitHub #8) are unchanged.
- **Mutable parameters**: `surface()`, `displacement()` and `atmosphere()` return a variable and a global index that the JIT uses to read the producing shader's current value (`jitParameterFinish`); `incident()` and `opposite()` skip mutable parameters. The v2 answer must carry the same information.
- **Light queries** (`lightsource()`): they read the light being iterated, which exists only inside an illuminance or illuminate loop; outside one, the result and diagnostic are today's.
- **Thread safety**: services are called from every render thread. Each v2 service states whether it is called concurrently; the renderer's implementations take no new lock on the hot path (SC-010).
- **Re-entrancy**: a service called from inside a service (a tracer that shades, which re-enters `shade()` with a new state) must work, as `trace` does today through `traceEx` (shading depth).
- **A batch with inactive lanes or a derivative tail**: the contract states, as the grid code does today, that derivative-offset vertices follow the real ones and are not counted in the point count.
- **Ray type of a point shaded for something other than rendering** (displacement-only, bound estimation, a displacement pass): its ray type and hit mode are defined, not left to the producer.
- **Build options**: builds without the JIT keep building and passing; the `.slo` halves of the double-based tests register only with the JIT on. Builds without OpenEXR are unaffected.
- **Plug-in boundaries**: shadeops (`dso`) and DSO-visible structures are not touched; this boundary is internal C++ and not part of the C plug-in ABI of Principle VIII rule 4.

## Requirements *(mandatory)*

### Functional Requirements

**The v2 services interface**

- **FR-001**: Shading MUST own one renderer-services interface, version 2, that evolves the existing one in place (no second interface, no parallel old one): it keeps every capability of the current interface that Shading still needs and adds the services below. The interface MUST carry a version constant (major, minor, patch) from its first commit, with a documented rule for what bumps each part, and the history of changes MUST be recorded next to it. The constant is not checked at run time (there is no external implementer); its job is to make a change visible.
- **FR-002**: The interface MUST be documented in its header and in the spec's contract notes: for each method the inputs, outputs, not-found result, thread-safety, whether it may re-enter Shading and the version that introduced it. An implementer who reads only the documentation MUST be able to write a conforming double.
- **FR-003**: The interface MUST be implementable without any renderer-core, hider or geometry type in its signatures: every object, shader instance, light and ray reaches a service as an opaque handle or a Shading-owned plain structure. The interface MUST NOT include a header from Hiders, Geometry or the renderer core.
- **FR-004**: The shading library MUST be usable with a standalone double of the interface (constitution rule 5). A double (or a small set) MUST exist in the tests that implements the whole interface with canned or table-driven answers, records every call, and has no inner implementation to forward to. The three existing forwarding wrappers MAY be kept.

**Attributes and message passing (V15 step 1, DB-018)**

- **FR-005**: `get_attribute` MUST answer `attribute()` and `option()` queries (and `rendererinfo()` where it reads the same data): the request names the shading point's object handle, the attribute or option name and the type the shader expects, and the answer is a raw typed value and an explicit status (found, not found, type mismatch); the type conversion and refusal master applies stay in Shading and run on that value. The set of names it knows MUST be the set `CShadingContext::attributes()` and `options()` know on master, plus user attributes and options as today.
- **FR-006**: `getmessage` MUST answer `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()` and `lightsource()`: the request names the source (the shader type attached to the object, or the current light), the parameter name and the expected type; the answer carries the value, the same explicit status and what the JIT needs to read a mutable parameter (variable and global index) exactly as `surfaceParameter` and its siblings return them today. The split is fixed (Clarifications, 2026-10-30): `get_attribute` carries `attribute()`, `option()` and `rendererinfo()`; `getmessage` carries only these six shader-to-shader queries.
- **FR-007**: The interpreter builtins MUST call the services for the builtins of FR-005 and FR-006, and so MUST the JIT: because both backends reach these through the same `CShadingContext` members, the change MUST be made inside those members (or the helpers they call), so that `.rslo` and `.slo` issue identical requests without an emitter change. Spec 033 keeps ownership of reshaping the JIT's message-passing entry points. `setmessage` and group-layer messages are not part of this spec (spec 034 adds them as a backward-compatible addition).

**Coordinate systems (`get_matrix`)**

- **FR-008**: `get_matrix` MUST return the from and to matrices for a coordinate-system name, given the shading point's object handle, the executing shader instance's handle, the current light's handle and a time. It replaces both `findCoordinateSystemWithType` and the object/shader/light resolution done inside Shading. The time parameter MUST be part of the contract (motion-blurred transforms, OSL), and the renderer's implementation MUST return the same matrix for every time, because master does; the time is carried in the signature only, and a test double may honour it; honouring it in the renderer is out of scope (see Assumptions).
- **FR-009**: All call sites of `findCoordinateSystem` (about 30) in the shading library MUST go through `get_matrix`. The diagnostics for an unknown system, an object reference without an object, and the colour-space names handled by the same lookup MUST stay as they are.

**Tracing (`ITracer`, V2(d))**

- **FR-010**: Shading MUST own an `ITracer` interface and reach it through the v2 services. It MUST be able to carry every request Shading issues today: a single ray that is not shaded (`trace(CRay *)`) and a bundle of rays that is shaded and post-processed (`traceEx(CRayBundle *)`), including the shading depth, the label, the post-shade callbacks and the sample identity each ray carries. The interface is expressed in the Shading-owned types that exist today (`CRay`, `CRayBundle`), and Shading's call sites go through it. It is installed per frame through the services (registered in `beginFrame()`, cleared in `endFrame()`, the calling context passed as a parameter); the plan MUST confirm that hiders running concurrently in one frame (the photon and camera passes) cannot overwrite each other. The hiders' existing trace code answers it unchanged behind an adapter. This spec claims no DB-001 or DB-005 allowlist line: bundle creation and the photon-map lookup are not moved here (spec 029). The OSL-shaped single-ray request is spec 049's adapter over this interface (Clarifications, 2026-10-30).
- **FR-011**: Every traced builtin (`trace`, `transmission`, `visibility`, `gather`, `occlusion`, `indirectdiffuse`, and the ray-traced forms of `environment` and `shadow`) MUST issue its rays through the tracer service, in the same order and with the same per-ray sample identity, depth, time, bias and differentials as on master, in both backends.
- **FR-012**: With no tracer installed, each traced builtin MUST warn once per kind and return the value a ray that hits nothing yields. A tracer MUST be allowed to re-enter Shading (it shades what it hits), as `traceEx` does.

**Texture and the other existing services**

- **FR-013**: The texture part of the v2 interface MUST be spec 027's, adopted unchanged: `textureSystem()` and the 3D-channel forwarders. This spec MUST NOT add a texture service of its own or change a texture signature. `getPhotonMap`, `getCache` and `getTexture3d` are not changed (spec 032).
- **FR-014**: Everything else in today's interface (frame and option getters, the illumination hook, filter/DSO/variable lookup, global IDs, offending-object reporting) MUST keep working with the same behaviour. The 56 methods are not regrouped in this spec: the v2 services are added as new members. The plan MUST classify each of the 56 methods as unchanged, grouped under the v2 services, or removed because a v2 service replaces it, and say why; a method is removed only when nothing in the tree calls it afterward.

**ShaderGlobals, ray type and hit mode (V3 step 1, V9 step 2)**

- **FR-015**: Shading MUST define `ShaderGlobals`, the description of a batch of shading points. It MUST be structure-of-arrays with a runtime point count and no fixed batch width (each per-point field a pointer or span plus a stride), and batch- or grid-shaped (a grid of u by v points, a span of independent points, or one point), so that a REYES grid and an OSL batch of 4, 8 or 16 map onto it without a later contract change. It MUST carry, per point: position, shading normal and geometric normal, the incident direction when there is one, parametric coordinates, the surface derivatives (`dPdu`, `dPdv`), the point's differentials (`dPdx`-style for ray hits and the grid's `du`/`dv` differences for grids), time and its derivative with respect to time (`dPdtime`), and the sample identity (spec 039's key). Per batch: an opaque object handle, an opaque shader-instance handle, the ray type, the hit mode, the path depth, the transport mode (radiance or importance) and the shading-cell footprint (the square root of the shading rate, in pixels, the same rule in every hider).
- **FR-016**: `ShaderGlobals` MUST state how absence of a field is expressed (for example no screen-space derivatives for a photon, no grid differences for a ray hit) and which fields a consumer may assume. The contract MUST define the derivative-offset tail and inactive lanes exactly as the grid code treats them today.
- **FR-017**: The ray type MUST be an explicit enumeration that names at least the camera ray, the secondary shading ray (reflection, refraction, gather, occlusion, indirect), the shadow or visibility ray, the photon (importance) ray and "no ray" (a grid shaded by a rasterizing hider, a bare point); the hit mode MUST name at least "shade the surface" and "primitive only" (today's `camerahitmode` values `s` and `p`). A table in the contract MUST map every situation in which the renderer shades today to its ray type, hit mode and transport mode, with the code location that determines it. The enumerations MUST allow additions as backward-compatible changes.
- **FR-018**: Nothing in the render path MUST produce or consume `ShaderGlobals`, the ray type or the hit mode in this spec: `shade()` keeps taking the primitive, the illumination hook and `camerahitmode` keep working as they do. Spec 029 switches `shade()` to the structure and reads the ray type. Tests MUST exercise the structure through a double producer and a double consumer, and MUST check the FR-017 table against the code's present inference.

**Registry and guardrails**

- **FR-019**: The architecture check MUST pass on macOS and Linux. The allowlist MUST only shrink: every line whose edge this spec removes is deleted, and no line is added or re-keyed. The plan MUST list, from a measurement on the starting tree, each line expected to go. DB-006's Shading-origin include edges (`execute.cpp` and `executeMisc.cpp` to `surface.h`) are expected to go once the attribute and matrix reads that need the primitive's type have moved behind the services; `shading.cpp` to `object.h` stays for spec 029's `shade()` switch. If the plan finds an edge cannot go, it records why.
- **FR-020**: `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` MUST be updated: DB-018's evidence, size and direction are edited to say the service part is done (with the spec number and date), what remains (the JIT's message-passing entry points, spec 033; group-layer messages, spec 034) stays Open under the entry; DB-006's counts and history record the reduction; DB-001 and DB-005 are not edited, because no edge of theirs moves in this spec (FR-010). An entry moves to Resolved only when it has no allowlist line and no finding left. No registry text, task or success criterion may refer to anything that exists only after the merge.
- **FR-021**: The renderer-core usage-metric baseline (`tests/architecture/metrics-baseline.txt`) MUST be lowered to the measured end values if any reference to the renderer core goes, and the Shading count MUST NOT rise.

**Behaviour and tests**

- **FR-022**: Rendering output MUST NOT change: byte-identical on macOS for every scene, `.rslo` and `.slo`, and the same pass/fail set as master on each platform. If the plan finds a place where the current behaviour differs between the two backends for a builtin this spec routes, it records the difference and keeps each backend's output (this spec fixes none).
- **FR-023**: Tests MUST be written first and shown failing on the tree before the change (constitution III): (a) a program that links the shading library, `libRIBCommon` and the double and no renderer object (User Story 1); (b) the attribute and message test under both backends against canned answers (User Story 3); (c) the `get_matrix` test (User Story 4); (d) the `ShaderGlobals` producer/consumer and situation-table test (User Story 5); (e) the tracer test (User Story 6); (f) a no-services test per new service. Existing tests MUST keep passing without edits beyond include paths and the doubles' base class.
- **FR-024**: The standalone-double test program MUST NOT link `ribRender`. Where `libshader_shading` still carries flat-namespace imports that dyld binds at load (the bundle vtables, `CGatherBundle`'s constructor, `CPhotonMap::lookup`), the test program MUST supply them from a stub that lives in the test directory and that the plan names symbol by symbol, or the imports MUST be gone; the plan measures which symbols a covered path actually needs on macOS and Linux. The stub is deleted by the spec that removes the imports (029). A test that depends on the stub being exercised MUST fail, not hang.
- **FR-025**: No task or text MAY depend on the merge. Issues close through the maintainer's merge message (`Closes #32`); this spec does not write it.

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

- **SC-001**: A test program that links the shading library, `libRIBCommon` and the services double, and no renderer-core, hider or geometry object, builds, installs the double and passes under the interpreter and (JIT on) the JIT on macOS and Linux (User Story 1, FR-004, FR-024).
- **SC-002**: Zero reads of the current object's attributes remain for `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()`, `lightsource()`, `attribute()` and `option()`, and zero uses of the primitive's transform remain for coordinate-system lookup; the plan lists the other attribute reads that stay and why (User Stories 3 and 4).
- **SC-003**: For each of the eight message and attribute builtins, an interpreted and a JIT-compiled shader send the double identical requests and receive its canned answers; the same shaders on the real renderer return master's values (User Story 3).
- **SC-004**: Every coordinate-system lookup goes through `get_matrix`; the transform builtins' results equal master's for every system kind (User Story 4).
- **SC-005**: A tracer double receives, for each traced builtin, the same ray sequence master traces (checked by a recorded replay in the manner of spec 028's motion replay, or by an equivalent exact comparison the plan chooses), under both backends (User Story 6).
- **SC-006**: The `ShaderGlobals` situation table covers every shading situation in the visual suite, and the test that reproduces the derivation passes; every field of the structure has documented presence rules (User Story 5).
- **SC-007**: On macOS, every scene of the visual suite, the parity suites and the determinism sweep renders to bytes identical to master's, and on each platform the full suite has master's pass/fail set (User Story 2, FR-022).
- **SC-008**: The architecture check passes on macOS and Linux; DB-006's allowlist lines fall by the measured count (the two Shading-origin `surface.h` include edges are expected), no line is added or re-keyed, and `Architecture_DomainBoundaries` reports no stale line (User Story 7).
- **SC-009**: `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` records DB-018's service part and DB-006's reduction with the spec number and date, and no entry refers to the merge.
- **SC-010**: A shading-heavy scene's render time on the finished tree is within the run-to-run noise measured on master, on the same machine and thread count (the services add a call where a field read was; a regression beyond noise is a defect, as in spec 027).
- **SC-011**: Shading's flat-namespace imports on macOS do not increase and are a subset of the starting set; the Linux counterpart does not grow.
- **SC-012**: The interface headers contain the version constant, the per-method documentation of FR-002 and the version history, and a reader who has seen only them can write a conforming double (reviewed against the test double by someone who did not write it).

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
- **Motion-blurred transforms and ray differentials as new features**: the time parameter and the differential fields exist in the contract; the implementation returns and fills what master does.

## Assumptions

- Specs 023, 024, 025, 027, 028 and 039 are merged on master (`d01da47`). The base of this spec is master; no rebase is assumed.
- The interfaces are internal C++ (plan decision, "026 stays internal C++"). Principle VIII rule 4's MAJOR/MINOR/PATCH discipline applies to plug-in boundaries, so there is no run-time version check; the constant exists so that a change to the contract is a visible, documented act. The public C facade is spec 052.
- The services interface evolves in place (one `CRendererServices`, version 2). It has one production implementer and three test classes, all in this repository, so no parallel old interface is kept. The names `get_matrix`, `get_attribute` and `getmessage` follow OSL's `RendererServices` for the adapter of spec 049; the exact C++ spellings, argument types and where the interface lives are the plan's decision.
- Because the interpreter and the JIT call the same `CShadingContext` members for message passing, routing those members through the services reaches both backends. Spec 033's later reshaping of the JIT entry points is unaffected.
- `ShaderGlobals` is a contract, not yet a data path (FR-018). Its field list follows the plan's brief (batch-shaped, ray differentials and grid differences, path depth, transport mode, shading-cell footprint) and reference document sections 5.5, 8.1 and 8.3. The layout is structure-of-arrays with a runtime point count (Clarifications, 2026-10-30); the allocation and the exact spellings are the plan's decisions.
- The path depth, the sample identity and the transport mode already exist in the renderer (`CRay::depth`, spec 039's key, the photon hider); this spec names them in the contract and does not add data.
- The cameras' and hiders' existing inference of the hit mode (`camerahitmode`, read only for grids) is preserved as the definition of today's behaviour, including its oddities; any oddity found while writing the situation table is recorded, not fixed.
- The current object, the shader instance and the light reach the services as opaque handles that Shading passes through unchanged; the renderer-core implementation converts them to its own types. Shading continues to hold a `CSurface *` until spec 029 (DB-006's `shading.cpp` to `object.h` edge).
- Texture, 3D-texture channel and `textureSystem()` access stay exactly as spec 027 left them (FR-013). Spec 027 defined the texture part of the interface; this spec adopts it.
- DB-018 has no allowlist line (it is not a dependency edge), so "allowlist lines of every resolved entry deleted" is met through DB-006's lines, and DB-018 stays Open for the JIT and group-layer parts; DB-001 and DB-005 lose no line in this spec (FR-010).
- Out of scope but found while reading (not fixed here): the JIT's `printf()` is a no-op dispatch case (GitHub #11), a JIT-wide uniform-conditional defect (GitHub #8), `setcomp`/`comp` index inconsistency (#9), and `transform()`'s matrix overload (#10).
- Linux is validated by the maintainer's rsync and Maestri path with a remote `cpujob` at `-j2`, as for specs 024 to 028; the Linux numbers are part of the plan's hand-off list.

## Open Questions

None remain; all questions were answered on 2026-10-30 (see Clarifications).
