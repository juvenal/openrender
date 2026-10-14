# Feature Specification: Shading Services Routing

**Feature Branch**: `024-shading-services-routing`
**Created**: 2026-10-13
**Status**: Draft
**Input**: Constitution Principle VIII remediation, layer L1, after spec 023. Route every shading-library call that bypasses the renderer-services interface through it, add a service for DSO shadeop lookup, and make the JIT backend honour the hiders' illumination hook exactly as the interpreter does. Reduces registry entry DB-001 (GitHub #24) by its renderer-core part and resolves DB-012 (GitHub #30).

## Clarifications

### Session 2026-10-13

- Q: With no renderer services installed (the shading library used on its own), what should a routed lookup do? → A: Report a diagnostic and return that lookup's normal "not found" result: no DSO, the default Gaussian filter (and Gaussian step filter), no category ID, no variable. Never crash, never treat it as fatal.
- Q: Is fixing the JIT's missing begin call for the no-argument `solar()` form in scope for this spec? → A: Yes, in scope. The plan confirms the gap from the compiler's intermediate code before any task depends on it.
- Q: Does DB-012 move to Resolved with this spec, or stay Open because its tracking line also names specs 026 and 029? → A: Resolved by spec 024, with the date, once its three evidence items are gone (interpreter-only hook check, JIT ignoring the hook, uncalled hook-aware wrappers) and the photon-pass parity test passes. Spec 026's ray-type work belongs to its own registry entries and issue #32, and DB-012's tracking line is edited to match.
- Q: With no services installed, how severe is the diagnostic, and how often is it reported? → A: A warning, reported once per kind of lookup per process (DSO, filter, step filter, global ID, variable). A missing category ID means the category is skipped, never stored as 0 (0 ends the category list).
- Q: The photon hider mishandles the no-argument `solar()` form even under the interpreter. Fix it here, or record it? → A: Record it, don't fix it in 024. Filed as GitHub #52 and listed as Open in `DEVNOTES_DETAILS/BUGS.md`. The fix gets its own `fix/` branch after 024 merges, and 024 must not depend on it.
- Q: Once DB-012 is Resolved, where is the ray-type follow-up recorded in the registry? → A: On DB-018's Direction line (the Open entry spec 026 resolves). The registry edit that resolves DB-012 moves the explicit ray type / hit mode item there. Issue #32 has already been updated outside this spec.

## User Scenarios & Testing *(mandatory)*

The "users" of this feature are the people who build, maintain and extend openRender: contributors working in the Shading domain, maintainers reviewing remediation specs, developers who embed or link the shading library, and anyone rendering photon maps with JIT-compiled (`.slo`) light shaders.

### Measured starting point (2026-10-13, master `690dbfc`, macOS)

- The shading library binds **12** symbols flat-namespace at load time ("dynamically looked up"). Five of them are renderer-core calls: `CRenderer::getDSO`, `getFilter`, `getStepFilter`, `getGlobalID` and `retrieveVariable`. The other seven belong to the Hiders and Texturing domains (`CShadingContext::trace`/`traceEx`, `CGatherBundle`, the `CTraceBundle`/`CTransmissionBundle` vtables, `CPhotonMap::lookup`, `CTexture3d::resolve`) and are out of scope (spec 029 and later).
- `parseVariable` is already resolved from the common library (spec 023) and is no longer a renderer-core dependency.
- The renderer-services interface already offers filter, step-filter, global-ID and variable lookups. The shading library bypasses it at **9 call sites in 3 sources** (light categories at shader-instance creation, texture/gather parameter binding, and the compiled-shader loader). No service exists for DSO shadeop lookup.
- The architecture check allowlists **18 edges** under DB-001. Ten of them are this spec's: 4 includes of the renderer-core header (`renderer.h`) from Shading sources and 6 link references to the five renderer-core calls. Shading's renderer-core usage metric is **9 references, 5 distinct members**.
- The interpreter checks the illumination hook in all six illuminate/solar opcodes (two `illuminate` forms, `endilluminate`, two `solar` forms, `endsolar`). The JIT's entry points for the same constructs never check it. Hook-aware JIT-side wrappers exist but nothing calls them.

### User Story 1 - JIT light shaders emit photons like interpreted ones (Priority: P1)

A user renders a scene whose photon pass uses JIT-compiled light shaders. Today the JIT never hands control to the photon hider's illumination hook. Measured 2026-10-14 (research R4): instead, `orender` crashes in the photon pass, with a null dereference in the JIT's `illuminate` begin. That code culls with the cone data (`costheta`) that only the lighting loop sets up, and the photon hider never runs that loop because it expects the hook to take over. So no photon map is written and the render fails. After this feature, a JIT light shader drives the photon pass the same way the interpreted version of the same shader does.

**Why this priority**: It is the only user-visible defect in scope. A photon pass with JIT lights crashes the renderer while the interpreter renders correctly, which breaks constitution rule 3: shading results must not depend on the backend.

**Independent Test**: Render a two-frame scene. Frame 1 is a photon pass that writes a global photon map from point, spot and distant lights. Frame 2 renders a surface that reads that map. Run it once with the light shaders pinned to the interpreter and once pinned to the JIT, with every other input identical, and compare the two Frame 2 images.

**Acceptance Scenarios**:

1. **Given** the parity scene with its photon-pass lights pinned to the JIT, **When** it is rendered on the tree before this feature, **Then** the parity test fails. Today it fails because `orender` exits nonzero (the crash measured in research R4). In general it fails whenever the render does not complete or the map-reading frame differs from the interpreter result beyond the threshold. This proves the test detects the defect.
2. **Given** the same scene on the finished tree, **When** it is rendered with the lights pinned to the JIT and then to the interpreter, **Then** the two map-reading frames agree within a threshold set from the interpreter's own measured run-to-run variance.
3. **Given** the parity scene, **When** it is inspected, **Then** its photon pass exercises every illumination form a shipped light uses: positional `illuminate` (point light), cone `illuminate` (spot light) and directional `solar` (distant light).
4. **Given** any hider that does not request the illumination hook (reyes, zbuffer, raytrace), **When** the existing JIT and interpreter scenes are rendered, **Then** their results are unchanged.

---

### User Story 2 - The shading library reaches the renderer only through its services (Priority: P1)

A developer links or loads the shading library. Today it calls five renderer-core functions directly, so the renderer library has to be loaded first for those symbols to resolve (DB-001, #24). After this feature, all five lookups go through the renderer-services interface that Shading owns and the renderer core implements. None of the five symbols remains among the shading library's undefined symbols, and no Shading source includes the renderer-core header directly.

**Why this priority**: It is the remediation this spec exists for in the registry. It removes the renderer-core half of the load-time coupling, so only the Hiders and Texturing bindings that spec 029 inverts remain.

**Independent Test**: On macOS and Linux, list the shading library's undefined symbols and the library each one binds to, run the architecture check, and compare both against the measured starting point.

**Acceptance Scenarios**:

1. **Given** the built shading library, **When** its undefined symbols are listed, **Then** `CRenderer::getDSO`, `getFilter`, `getStepFilter`, `getGlobalID` and `retrieveVariable` are absent, on both platforms.
2. **Given** the same list, **When** it is compared with the starting point, **Then** the only renderer-library symbols left unresolved at link time are the seven Hiders/Texturing symbols listed above ("dynamically looked up" on macOS; undefined and provided by no `NEEDED` library on Linux), and no new one has appeared.
3. **Given** the finished tree, **When** the architecture check runs, **Then** it passes, the 10 DB-001 allowlist lines for this spec's edges are gone, and no allowlist line was added.
4. **Given** the finished tree, **When** the renderer-core usage metric is reported, **Then** Shading's count is 0 references and 0 distinct members, and the baseline records that.

---

### User Story 3 - Shaders load and bind identically, whenever they are loaded (Priority: P2)

A user renders scenes whose shaders use DSO shadeops, named texture or step filters, light categories, or `surface:` gather outputs, including shaders loaded before the first frame begins (Imager shaders, and the default surface and light bound at `WorldBegin`). Today these lookups bypass the services and always work. Routed through the services, they must keep working at every point where a shader can be loaded or bound, including the first frame of a session.

**Why this priority**: Routing the calls is structurally simple, but the services are installed only when a frame begins, after some shaders have already loaded. Without this story the remediation would silently change behaviour for those shaders.

**Independent Test**: Render a scene that loads, before its first frame begins, a shader using at least one routed lookup, and compare it with the same scene on the starting tree. Separately, exercise the shading library with a substitute services implementation and confirm that each routed lookup is answered by it.

**Acceptance Scenarios**:

1. **Given** a shader loaded before the first frame begins that uses a routed lookup, **When** the scene renders, **Then** the lookup succeeds and the image matches the starting tree's.
2. **Given** a substitute services implementation installed in a test, **When** the shading library performs each of the five routed lookups (DSO, filter, step filter, global ID, variable), **Then** each one is answered by the substitute and none reaches the renderer core.
3. **Given** the full test suite, **When** it runs on the finished tree, **Then** results are unchanged and renders that were deterministic before are byte-identical.

---

### Edge Cases

- **Shaders loaded before the first frame**: Imager shaders load before `WorldBegin`, and the default surface and light load inside `WorldBegin` before the frame's services are registered. Every routed lookup must have services available at those points, in the first frame of the first session and in every later session of the same process.
- **No services installed** (the shading library used on its own, through its public C API, with no renderer): a routed lookup must never dereference a missing services object. Instead it returns that lookup's normal "not found" result: no DSO, the default Gaussian filter or step filter, no category ID, no variable. It reports a warning once per kind of lookup per process, and rendering continues. A missing category ID means the category is skipped, never stored as 0, because 0 ends a shader's category list. The public headers that today promise a "CRenderer::* fallback" must describe this behaviour.
- **Unknown names**: an unknown DSO, filter, step-filter or variable name gives the same result and the same diagnostic as today (filters fall back to Gaussian, as now).
- **Repeated lookups and thread safety**: DSO lookup caches loaded shadeops and today is called only from the single-threaded shader loader; the routed version must keep that behaviour and must not add locking cost to per-sample shading paths.
- **Hook toggled between frames**: in a multi-frame RIB where the photon pass is followed by another hider, as in the parity scene, each frame's backend decision reads that frame's hider. The JIT must never cache the hook state across frames or inside compiled code.
- **Hook active, all vertices inactive**: with the hook active, the interpreter hands the whole construct to the hider and does not apply its own cone/back-face culling or jump past the body. The JIT must do the same: no culling, no skipped body, and the shading-state active/passive counts left as the interpreter leaves them.
- **`solar()` with no arguments**: the interpreter handles this form, calling the hook when it is active and otherwise setting `L` from `Ps`. The JIT emits no begin call for it at all: the instruction's only operand is its label, so no direction operand is found. This is confirmed from the intermediate code (research R1): the `.rslo` line is `solar #!Label2`, the JIT bitcode has no `op_solar_begin` but does have `op_solar_end`, and a camera render differs from the interpreter by a MaxBlockAvgDiff of 201.62. No shipped shader uses this form. Fixing the JIT's handling of it is in scope (FR-009, FR-010). The photon hider's own handling of the form is out of scope, under either backend: its begin hook does nothing for a missing direction, and its end hook traces from stale `L`, `Ps` and `powerScale`. That is GitHub #52, recorded in `DEVNOTES_DETAILS/BUGS.md` and fixed on its own branch after this spec. Nothing in this spec depends on that fix.
- **Known photon-hider crash**: `DEVNOTES_DETAILS/BUGS.md` lists an Open, pre-existing intermittent SIGSEGV in `CPhotonHider` photon-map construction (about 10% of `Visual_subdiv-loop-photon` runs). The photon-pass parity test (FR-012) must not hide it, for example by retrying, and must not be made flaky by it. It was measured on the parity scene with the interpreter on macOS (research R4): 4 of 30 runs crashed at the default thread count, and 0 of 30 with one thread (`-t:1`), which was also bit-for-bit deterministic. The test therefore renders with one thread. That avoids the race without masking it: the race stays recorded in BUGS.md and is fixed separately. Fixing that crash is out of scope.
- **Ambient lights**: light shaders with no illuminate/solar construct are unaffected by the hook under both backends.
- **Build options**: a build without the JIT still builds and passes. The new parity test is registered only when the JIT is available, like the existing `-slo` scenes.

## Requirements *(mandatory)*

### Functional Requirements

**Services routing**

- **FR-001**: Every shading-library lookup of a filter function, a step-filter function, a global (light-category) identifier or a declared variable MUST go through the renderer-services interface. None may call the renderer core directly.
- **FR-002**: The renderer-services interface MUST gain a DSO shadeop lookup (by shadeop name and prototype) that the renderer core implements with today's behaviour: same search path, caching, initialization and "not found" result. The shading library's compiled-shader loader MUST use it.
- **FR-003**: The DSO handle the shading library stores and calls MUST be described by a type the Shading domain may depend on: a Shading-owned header, not the renderer-core header. Any change to the published shadeop plug-in header is out of scope (DB-003/DB-014).
- **FR-004**: No Shading source MAY include the renderer-core header directly. Shading code MUST NOT use any declaration that only the renderer-core header provides. A grep of the Shading sources on 2026-10-13 found exactly three in code (not comments): `CRenderer` members, the DSO handle type (`CDSO`) and the illumination-hook bit (`HIDER_ILLUMINATIONHOOK`). This holds even where the header still arrives transitively through Geometry headers registered under DB-006 (Geometry's `object.h` includes it), which stay until spec 031. Anything Shading needs comes from Shading-owned or infrastructure headers, or from the services interface.
- **FR-005**: Renderer services MUST be registered before the first point at which any shader can be loaded or bound in a session (before Imager shaders, and before the default surface and light are bound at `WorldBegin`), and MUST stay registered for as long as shaders can be loaded or executed. Registering again for each frame MUST stay harmless.
- **FR-006**: With no renderer services installed, every routed lookup MUST return its normal "not found" result: no DSO, the default Gaussian filter or step filter, no category ID, no variable. It MUST report a warning once per kind of lookup per process: at most one each for DSO, filter, step filter, global ID and variable, however many lookups fail. A missing category ID MUST make the category be skipped, never stored as 0. No routed lookup may dereference a missing services object or end the process. The public shading-library headers MUST describe this behaviour and drop the "CRenderer::* fallback" wording for these lookups.
- **FR-007**: No variable-parsing service is added. Variable-declaration parsing is infrastructure provided by the common library since spec 023, which Shading may depend on.

**Illumination hook parity**

- **FR-008**: Both backends MUST decide "the hider takes over illumination" through one query, the existing illumination-hook capability of the renderer-services interface, evaluated per construct execution. The interpreter MUST stop testing the renderer-core hider-flag bit itself.
- **FR-009**: With the hook active, the JIT MUST behave like the interpreter for every illumination construct a JIT shader can contain: hand the begin and end of each positional `illuminate`, cone `illuminate` and `solar` construct to the hider's hooks with the same arguments, and skip its own culling and light saving, exactly as the interpreter does. This includes the no-argument `solar()` form: with the hook active, the JIT makes the same begin and end hook calls the interpreter makes, with no direction and no angle.
- **FR-010**: With the hook inactive, JIT behaviour MUST be unchanged, with one exception: JIT shaders using the no-argument `solar()` form MUST now match the interpreter, which sets `L` from `Ps` for every active point.
- **FR-011**: When the feature is finished, exactly one hook-aware implementation of each JIT illumination entry point MUST be in use. No uncalled duplicate (today's hook-aware wrappers) may remain: either it becomes the JIT path or it is removed.

**Tests**

- **FR-012**: A new backend-parity test MUST cover the photon pass. With every input identical except the backend of the photon-pass light shaders, it compares a later frame that reads the photon map written by that pass. It MUST include a point, a spot and a distant light. It MUST fail on the tree before the JIT change (red first) and pass after it. Its threshold MUST be derived from the interpreter's own measured run-to-run variance, using deterministic sampling or a single thread if needed, and MUST NOT be widened to pass. This is parity between two backends of one shader inside the photon pass, not photon-vs-camera-hider parity.
- **FR-013**: A test MUST cover a shader loaded before the first frame begins that uses a routed lookup (FR-005).
- **FR-014**: A test MUST exercise the shading library against a substitute renderer-services implementation and show each of the five routed lookups is answered by it (constitution rule 5). The same test MUST cover the no-services case (FR-006). Performing each kind of lookup more than once with no services installed MUST return the "not found" result every time, MUST report exactly one warning per kind (five in total), MUST skip a missing category rather than store 0, and MUST NOT crash.
- **FR-019**: A backend-parity test MUST cover the no-argument `solar()` form under a camera hider (hook inactive): a light shader using it renders the same under the JIT as under the interpreter. Today it is expected to fail (red first). With the hook active, the requirement is only that the JIT makes the same hook calls (FR-009). How the photon hider handles this form (GitHub #52) is unchanged, not compared, and out of scope.

**Registry and guardrails**

- **FR-015**: The architecture check MUST pass on macOS and Linux. The DB-001 allowlist lines for the 4 renderer-core header includes and the 6 renderer-core link references MUST be deleted. Any other line the change makes stale MUST be deleted. No line may be added.
- **FR-016**: The renderer-core usage metric baseline for Shading MUST be lowered to the new values (0 references, 0 distinct members) and MUST NOT rise for any class.
- **FR-017**: The registry MUST be updated. DB-001 stays Open: its evidence lists the seven Hiders/Texturing symbols that remain, and its edge count and history record this spec's reduction with the spec number and date. DB-012 moves to Resolved by spec 024, with the date, once all three of its evidence items are gone (interpreter-only hook check, JIT ignoring the hook, uncalled hook-aware wrappers) and the photon-pass parity test passes. Its tracking line is edited to name only spec 024 (#30). The same registry edit moves the explicit ray type / hit mode item (spec 026, #32) from DB-012 to DB-018's Direction line, so the follow-up stays recorded under an Open entry. No registry text may refer to anything that exists only after the merge. Issues close through the maintainer's merge message.
- **FR-018**: Rendering output and existing test results MUST NOT change, except that JIT light shaders now emit photons in photon passes, and JIT shaders using the no-argument `solar()` form now match the interpreter. The full suite MUST stay green on macOS and on Linux (GCC 13, warnings as errors).

### Key Entities

- **Renderer services**: the Shading-owned interface through which the shading library asks the host renderer for resources and lookups. The renderer core implements it and registers it.
- **Routed lookups**: DSO shadeop (name and prototype to handle), filter and step-filter function (name to function), global identifier (category name to integer ID), declared variable (name to declaration).
- **DSO handle**: a loaded shadeop: module handle, init/exec/cleanup entry points, name and prototype.
- **Illumination hook**: a capability a hider requests (today only the photon hider). When it is active, the begin and end of every illuminate/solar construct are handed to the hider instead of the shading engine's own culling and light saving.
- **Photon-pass parity scene**: a two-frame scene whose first frame writes a photon map from point, spot and distant lights and whose second frame reads it. It is rendered once per light-shader backend.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On macOS and Linux, none of the five renderer-core symbols (`getDSO`, `getFilter`, `getStepFilter`, `getGlobalID`, `retrieveVariable`) is undefined in the shading library. On macOS the count of "dynamically looked up" imports drops from 12 to 7, and the 7 are exactly the Hiders/Texturing symbols listed in the starting point. On Linux the counterpart is the set of strong undefined (`U`) symbols in the shading library's dynamic symbol table that bind to the renderer library at run time. Vague-linkage symbols (inline functions and variables, which both libraries define and which ELF interposition binds to whichever loads first) are excluded. The set was measured before any change at 2026-10-14: exactly the same 12 as on macOS (research R3). It must shrink by exactly the five symbols, to 7.
- **SC-002**: The architecture check passes on both platforms with DB-001 down from 18 to 8 allowlisted edges, zero direct Shading includes of the renderer-core header, and no allowlist line that was not there before.
- **SC-003**: Shading's renderer-core usage metric is 0 references and 0 distinct members, down from 9 and 5.
- **SC-004**: The photon-pass parity test fails on the tree before the JIT change and passes on the finished tree, with a threshold no wider than the measured interpreter run-to-run variance justifies.
- **SC-005**: In the parity scene, the JIT photon pass completes and stores a non-zero number of photons. Before this feature it crashes in the photon pass and writes no map (measured 2026-10-14, macOS, at the default thread count and at `-t:1`). Agreement with the interpreter is judged by SC-004.
- **SC-006**: The full test suite passes on both platforms with the same results as before, apart from the new tests and the exceptions FR-018 allows. Renders that were byte-identical across two runs of the starting tree stay byte-identical.
- **SC-007**: No hook-aware illumination entry point in the shading library is left without a caller.
- **SC-008**: DB-012 is listed under Resolved with spec 024 and its date, its tracking line names only #30, none of its three evidence items can still be reproduced in the tree, and DB-018's Direction line carries the explicit ray type / hit mode item.
- **SC-009**: The no-argument `solar()` backend-parity test (FR-019) passes on the finished tree. With no services installed and each kind of lookup repeated, all five kinds return their "not found" result every time, exactly 5 warnings are reported (one per kind), a missing category is skipped, and nothing crashes.

## Assumptions

- Spec 023 is merged. `libRIBCommon` provides diagnostics, vocabulary, `CVariable`/`parseVariable` and the sampling helpers. Spec 022's architecture check, allowlist, registry and metric are the yardstick.
- The brief asked for "service methods for DSO lookup and variable parsing". Only the DSO service is new: variable parsing is infrastructure since spec 023 (the registry's DB-001 history says so), and declared-variable lookup already has a service method that the shading library bypasses today.
- The seven Hiders/Texturing symbols (trace, gather/trace/transmission bundles, photon-map lookup, 3D-texture resolve) and their 8 allowlist edges stay. They belong to spec 029 (DB-001/DB-005). The hider headers Shading includes (DB-005), the Geometry coupling (DB-006) and the compiler's own `dso.h` includes (DB-019) are out of scope.
- The renderer core's own direct use of the hider flags, for the built-in sphere and quad lights (DB-013), is out of scope.
- The photon hider's handling of the no-argument `solar()` form (GitHub #52) is out of scope. It is recorded as Open in `DEVNOTES_DETAILS/BUGS.md` and fixed on its own branch after this spec merges.
- The JIT still has no DSO shadeop call path (DB-019, spec 037). This spec routes only the interpreter's DSO lookup.
- A JIT DSO path, the `ShaderGlobals`/ray-type work and `RendererServices` v2 are spec 026 and later.
- The parity test reads the stored photons through a rendered frame that looks up the photon map, measured with the visual suite's existing block-average metric. Comparing photon-map contents directly is an acceptable addition, not a replacement.
- Checking the hook once per construct execution through the services adds no measurable render-time cost, so no performance criterion is set. A regression found during implementation is treated as a defect.
- GitHub #24 (DB-001) stays open after this spec. #30 (this spec, DB-012) is closed by the maintainer's merge message.
