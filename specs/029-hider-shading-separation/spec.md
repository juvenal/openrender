# Feature Specification: Hider / Shading Separation

**Feature Branch**: `029-hider-shading-separation`
**Created**: 2026-11-01
**Status**: Draft
**Input**: Constitution Principle VIII remediation, layer L3/L4, after specs 026 and 027 (both merged); GitHub #35, with #24 folded in (maintainer decision, 2026-11-01). Hiders stop inheriting Shading's `CShadingContext` and own one instead; the hiders implement the Shading-owned `ITracer` themselves and `trace*` leaves Shading; `shade()` consumes `ShaderGlobals`; the bundle classes, the photon-map lookup and the illumination hooks reach Shading through interfaces Shading owns. The point of all of it is the link: `libshader_shading` ends with zero unresolved symbols, so the `-undefined dynamic_lookup` / `--allow-shlib-undefined` wiring and the `-force_load` / `--whole-archive` workaround of the CMake package are removed and a static consumer launches without them. Resolves registry entries DB-005 and DB-001 (the remainder), the Shading half of DB-020, and reduces DB-006 and DB-011. Renders stay byte-identical.

## Clarifications

### Session 2026-11-01 (maintainer, before specify)

- Q: What happens to #24 (the `libshader_shading` <-> `libRIBRender` coupling)? -> A: It is folded into this spec and closed by it (the `Closes #NN` line is the maintainer's, FR-032). DB-001's measured remainder (4 flat-namespace imports, listed below) is this spec's, and so is the static-consumer launch failure #24 describes. (maintainer)
- Q: Acceptance wording? -> A: `libshader_shading` links with undefined symbols as an error and has no flat-namespace imports; the `-force_load` workaround is removed from the CMake package; a static consumer launches without it on macOS and Linux; the allowlist lines of every resolved entry are deleted, `Architecture_DomainBoundaries` passes, fully resolved entries move to Resolved; renders are byte-identical. (maintainer, #35)

### Session 2026-11-01

The specify stage raised three questions; the maintainer answered each with the recommended option. The spec text below is amended to match.

- Q1: What is the input of `shade()`, given that it calls the primitive back after it has chosen the shaders (FR-018, FR-021)? -> A: Option B. `shade()` takes `ShaderGlobals` plus a Shading-owned point-source interface with the two operations it needs from the primitive (re-sample a range of points; interpolate the primitive variables into the shader locals). A Hiders-side adapter over the unchanged `CSurface::sample`/`interpolate` implements it. Shading keeps the derivative-offset arithmetic and the call order exactly. The producer lives in Hiders, not in Geometry. The fully self-contained OSL shape (the producer builds the derivative tail and the batch carries the primitive variables) is left for spec 049. (maintainer)
- Q2: How does Shading reach the photon map without naming `CPhotonMap` (FR-012)? -> A: Option A. A small Shading-owned abstract photon-map lookup interface that `CPhotonMap` implements and `CRendererServices::getPhotonMap` returns. Storage, file handling and balancing stay in Hiders until spec 032. The services interface changes a member's signature (FR-015). (maintainer) *Amended 2026-11-02 (versioning policy, below): the change takes `kRendererServicesVersion` from 2.0.0 to 2.1.0.*
- Q3: Is the arena and statistics injection that DB-016's Resolved text leaves "with spec 029" in this spec (D5)? -> A: Option A. Out of 029. The registry edits correct DB-016's sentence so it no longer names spec 029, and the follow-up is filed as #135 ("Inject the global arena and stats as per-context state (DB-016 follow-up, out of spec 029)"). (maintainer)
- #134 -> spec 033 is confirmed, and decisions D1 to D4 stand as written. (maintainer)

### Session 2026-11-02 (plan-stage decisions, maintainer)

The plan stage raised sixteen questions (plan.md, "Decisions"; research.md R1 to R16). The maintainer accepted the recommended option of every one, Q1 to Q16, and set the versioning policy below. The text of this spec is amended to match where an answer changed it.

- Q1 -> (a): Geometry keeps calling `CShadingContext::shade(CSurface *, ...)`, `displace()` and `displaceEstimate()` by their unchanged symbols. They stay in Shading as a thin forwarding entry that treats the `CSurface *` as opaque and hands it to the owning hider's Shading-owned `IShadingProducer`, until spec 031 retires the entry with `IDicer`/`IRayContext` (FR-018, US7 AS2).
- Q2 -> (a): the context holds its owner's tracer; the services' `tracer()` is the fallback for a context with no owner; the renderer's services answer `nullptr` (FR-016).
- Q3 -> (a): the shading state holds the shaded object as an opaque surface pointer, its `ObjectHandle` and its attributes, set together through `setShadedObject()` (FR-018, FR-025).
- Q4 -> (a): `shade()`'s input carries the per-batch fields only; `BatchShape::PointSet` is appended for 0-D batches and `Span` means 2-D ray hits; the requested parameters travel with the point source.
- Q5 -> (a): `classifyShading()` is corrected to the code in three places (a 0-D shadow hit reads `transmissionhitmode`; a secondary hit in a raster frame is `Secondary` and reads `specularhitmode`; only the emission batch itself is classified `Photon`), and `ShadingContract_Situations` is edited to match (D1, FR-019, US7 AS1, SC-008).
- Q6 -> (a): `CRenderer::contexts` becomes `CHider **` and keeps its name. Q7 -> (a): the core's dispatcher sets `activeContext` for every hider.
- Q8: superseded by the versioning policy below. Q9 -> the bundle files move into `src/libshader/shading/`. Q10 -> the names of research R16. Q11 -> `CShadingContext` is `final`, and the context offers a read-only `vertexMemoryInUse()`. Q12 -> named public members. Q13 -> labels `link` and `link-package`. Q14 -> (a): the installed-package consumer is an opt-in test (SC-002, FR-004). Q15 -> every include directory the compile allows goes. Q16 -> (a): the object hash stays in the context behind `objectHash()`.
- **Versioning policy** (maintainer, replaces Q8): MAJOR is a breaking change, MINOR a non-breaking addition, PATCH a bug fix only, but **MAJOR stays at 2 until spec 055**, because nothing versioned has been distributed: the milestone ends at MAJOR 2 (MAJOR 1 is the pre-versioning interface). Spec 029 takes `kRendererServicesVersion` from 2.0.0 to **2.1.0**, and the header's version history states that 2.x is unstable until the milestone closes (spec 055), so a MINOR step before then may break. Every implementer's `static_assert` stays on MAJOR 2 (FR-015).
- **Tasks-stage decisions (maintainer, 2026-11-02)**: the test edits found at the tasks stage are made (FR-025 above, corrections C7 and C8; no new `CRenderer` accessor); the eight departures D1 to D8 of plan.md ("Departures taken at the tasks stage") are accepted, among them the temporary `OPENRENDER_029_RED_TESTS` option removed in Phase 8 and the permanent opt-in `OPENRENDER_LINK_PACKAGE_TEST`.
- The plan's three out-of-spec findings (the DB-015 header count, the sweep count in CLAUDE.md, DB-011's evidence count) are fixed on master, not in this spec.

### Decisions recorded at the specify stage (confirmed by the maintainer, 2026-11-01)

These are not open questions; they are the placements the specify stage chose, with the reason, so a reviewer can overrule one cheaply.

- **D1**: #117 (`camerahitmode` honoured only for REYES grids) and #118 (`diffusehitmode` never read) are **out**. Fixing either changes images; byte-identical renders are required. 029 fills `rayType` and `hitMode` so that today's behaviour, oddities included, is reproduced, and the situation table of spec 026 (`classifyShading()`), **as corrected to the code by this spec** (Q5 of the plan stage: three cases where the function disagreed with `shade()`; it is test-only, so no image changes), is the oracle. After 029 each of the two is a one-place fix.
- **D2**: #119 (implicit-surface ray hits shaded 0-D) is **out**, and belongs to spec 031 (a Geometry source, an image change). #120 (three behaviours to verify against the RISpec) stays a verification issue; 029 neither fixes nor closes it. #123 is closed and needs nothing.
- **D3**: #134 (bind-time coordinate systems in `src/ri/render/init.cpp` bypass `getMatrix`) is **out**, owned by spec 033 (DB-007, shader binding and loading move into Shading). It is the core's own `CRendererContext::init` re-instantiating Shading's macro bodies (`init.cpp:60`), it is compiled in the core, and 029 touches neither shader binding nor that file's lookup. 029 is only required to keep `init.cpp` compiling.
- **D4**: The hcshader lights (`CSphereLight`, `CQuadLight`, `src/ri/hcshader.cpp`) are **not retired** here (spec 030, DB-013). Their `friend` access to `CShadingContext` goes because the illumination hooks they call (`hcshader.cpp:120,135,435,450`) become a public, Shading-owned interface.
- **D5**: The arena and statistics injection that DB-016's Resolved text leaves "with spec 029" is **out**, by the maintainer's answer to Q3 (Clarifications above); the registry sentence is corrected (FR-029) and the follow-up is #135.

## User Scenarios & Testing *(mandatory)*

The "users" are the people who build, maintain and extend openRender: contributors to Shading and to the hiders, maintainers reviewing Principle VIII remediation, the authors of the specs that build on this separation (030 hider registry, 031 geometry hider-agnostic, 032 GI cache split, 033 shading-system backends, 049 the OSL backend, 052 the public runtime library), and anyone who links the renderer into their own program, statically or not.

### Measured starting point (2026-11-01, master `d6910bd0`)

Method: static reading of the source, the allowlist and the spec 026 records in this worktree; `grep` counts. No build was run for the specify stage, nor for the plan stage. Phase 1 re-measures with `archcheck`, `dyld_info` and `nm` on a fresh macOS build, and on Linux over ssh (the per-spec tree and remote `cpujob -j2` of spec 026, Q13) (corrected at plan, 2026-11-02: C6). Where the registry text and the code disagree, the code is cited and the registry text is listed for correction (FR-031).

- **The hiders are `CShadingContext`s.** `CReyes` (`src/ri/hiders/reyes.h:106`), `CRaytracer` (`raytracer.h:145`) and `CPhotonHider` (`photon.h:41`) inherit `CShadingContext`, declared in Shading (`src/libshader/shading/shading.h:343`); `CStochastic` and `CZbuffer` inherit through `CReyes` (`stochastic.h:40`, `zbuffer.h:39`). No `CHider` class exists. The registry's `reyes.h:104` / `raytracer.h:140` are stale.
- **The Shading class has virtuals.** Six: the destructor (`shading.h:363`), `renderingLoop() = 0` (`:366`) and the four illumination hooks `solarBegin`/`solarEnd`/`illuminateBegin`/`illuminateEnd` (`:1007-1010`). `renderingLoop` is the hider's main loop and is called by the core (`renderer.cpp:1052`); the hooks are overridden by `CPhotonHider` (`photon.h:53-56`) and by a test probe (`tests/shading_parity/test_illumination_hook_operands.cpp:146`). The class is abstract, so Shading cannot construct one itself; nine test classes subclass it (corrected at plan, 2026-11-02: C1; the specify stage counted five). Five supply only an empty `renderingLoop` (`tests/shading_contract/contract_harness.h:38`, `rendered_harness.h:38`, `tests/shading_parity/test_context_no_services.cpp:75`, `texture_substitute_harness.h:56`, and `test_state_services.cpp:64`, which also reads the protected `vertexMemory` through accessors of its own, `:64-70`); four also override the illumination hooks (`test_illumination_hook.cpp:131`, `test_illumination_hook_conditions.cpp:127`, `test_illumination_hook_operands.cpp:146`, `test_false_if_constructs.cpp:62`).
- **Trace bodies live in Hiders and read Shading's private state.** `CShadingContext::trace(CRayBundle *)`, `traceEx` and `trace(CRay *)` are declared in Shading (`shading.h:391-393`) and defined in `src/ri/hiders/trace.cpp:47, 253, 303`. `traceEx` reads and writes `currentRayDepth`, `currentRayLabel`, `freeStates` and calls `newState()`; `trace(CRayBundle *)` uses `traceObjectHash` (`trace.cpp:98-147, 254-273`), all private members (`shading.h:1026-1031`), the last allocated by the class itself (`shading.cpp:448-452`). `traceAny` is declared (`shading.h:409`) and defined nowhere. Spec 026 put the Shading-owned `ITracer` in front of these (`tracer.h`), with a stateless adapter `CHiderTracer` (`src/ri/hiders/hiderTracer.{h,cpp}`) that forwards to the context's members, and a per-frame slot `CRenderer::frameTracer`.
- **Shading constructs Hiders classes and calls Hiders code.** `executeMisc.cpp` builds `CTransmissionBundle` and `CTraceBundle` on the stack (`:703, :845`), `shading.cpp` news a `CGatherBundle` (`:4426`), and `giFunctions.h:298,344` and `CShadingContext::jitPhotonMap` (`shading.cpp:4004`) call `CPhotonMap::lookup`. The three bundle classes (`src/ri/hiders/bundles.{h,cpp}`) contain Shading logic only: `bundles.cpp` includes just `bundles.h`, `common/diagnostics.h`, `shaderPl.h` and `common/stats.h`, and `bundles.h` includes `ray.h` and `shading.h`. Outside Shading its users are `irradiance.cpp` and `irradianceDispatch.cpp` (Hiders) and three core files (`renderer.cpp:34`, `rendererContext.cpp:39`, `rendererStatics.cpp:38`).
- **Shading includes Hiders headers: DB-005's 8 edges.** `bundles.h` from `execute.cpp`, `executeMisc.cpp`, `shaderPl.cpp`, `shading.cpp`; `irradiance.h` from `execute.cpp:38` and `shading.cpp:38`; `photonMap.h` from `execute.cpp` and `shading.cpp`. No identifier declared by `irradiance.h` (`CIrradianceCache`) appears in either file, so those two includes look vestigial (to confirm by a build in Phase 3; the plan stage ran no build).
- **`libshader_shading` imports 4 symbols through the flat namespace** (macOS, `specs/026-shading-contract-v2/tools/libshader-imports-merged-macos.txt`; the same 4 among the 465 `nm -D --undefined-only` lines on Linux, `-merged-linux.txt`): `CGatherBundle::CGatherBundle()`, `vtable for CTraceBundle`, `vtable for CTransmissionBundle`, `CPhotonMap::lookup(float*, float const*, int)`. They are DB-001's 5 allowlist lines (`allowlist.txt:14-18`). The vtables are data imports that dyld binds eagerly, so the library cannot be loaded without a definer.
- **The link wiring that exists because of them.** `src/libshader/shading/CMakeLists.txt:85` (`-undefined dynamic_lookup`, macOS), `:100` (INTERFACE `-Wl,--allow-shlib-undefined`, Linux, pushed onto every consumer), the file header comment (`:11-14`), a comment at `:103-105` and the include path that lists `src/ri/hiders`, `src/ri/geometry`, `src/ri/render` and others (`:106-119`, DB-023) (line numbers corrected at plan, 2026-11-02: C3); `CMake/openRenderConfig.cmake.in:79-97` (`-force_load` / `--whole-archive` / `--export-dynamic` on `RIBRender_static`, with a comment that names the hider symbols); the same block copied into `tests/unit/common/CMakeLists.txt:77-84`; the paragraph at `INSTALL_ARTIFACTS.md:89`; DB-001's "Consequences today". `src/common/CMakeLists.txt:80` already links `libRIBCommon` with `LINKER:--no-undefined` on Linux (spec 023); spec 023's research found that spelling out `-undefined error` is deprecated on current ld64 (it is the default for dylibs).
- **Tests that exist because of it.** `tests/shading_contract/load_stub.cpp` (65 lines, four trapping symbols), the `ribRender` link and `--no-as-needed` of four targets, `test_shading_service_lookup`, `test_shading_no_services`, `test_shading_context_no_services` and `test_shading_state_services` (`tests/shading_parity/CMakeLists.txt:316-326, 338-346, 360-368, 381-389`), the `ribRender` link of `LibShader_McompConsistency`, which exists only for the imports (`src/libshader/tests/CMakeLists.txt:88-112`) (corrected at plan, 2026-11-02: C2; the specify stage listed two targets at `:318-345`), and GitHub #44 (`LibShader_McompConsistency` cannot load on Linux; one of the 7 known Linux failures in spec 026's validation record).
- **Shading calls back into the primitive in the middle of `shade()`.** `shade()` (`shading.cpp:852`) takes a `CSurface *` and calls `object->sample()` and `object->interpolate()` (`:972-973, :1074, :1142-1145, :1163-1166, :1230-1233`). On the ray-hit path with derivatives it samples twice: Shading computes new (u, v) for the two derivative-offset points from the first sample's `dPdu`/`dPdv`/`I`/`du` (`:1076-1139`), then samples the primitive again at them (`:1142`). `interpolate()` writes the primitive's variables into the shader locals that `prepare()` allocates after the shaders are chosen (`:1032-1037`; e.g. `CSphere::interpolate`, `quadrics.cpp:517`). Spec 026's `ShaderGlobals` (`shaderGlobals.h`) is a passive description that nothing in the render path produces or consumes (FR-018 there); it has no field for primitive variables and expects the producer to build the tail (spec 026 `data-model.md`, "Derivative tail").
- **Who calls `shade()`.** Hiders: `reyes.cpp:907, 968, 1061, 1115` (inherited member). Geometry and the core: `CDLObject::shade` (`dlobject.cpp:173`), `CImplicit::shade` (`implicitSurface.cpp:242`), `CSurface::shade` (`render/geometryDispatch.cpp:405`), and `displace`/`displaceEstimate` from `surface.cpp:304-334, 1706-1794` and `geometryDispatch.cpp:455, 492`. DB-006's 7 link lines name `CShadingContext::shade`, `saveState` and `restoreState` as called from `dlobject.cpp`, `implicitSurface.cpp` and `surface.cpp`, and `CReyes::drawObject`/`drawGrid` from `surface.cpp`.
- **Geometry reaches the context through a `CReyes *`** (measured at plan, 2026-11-02: C5): inherited members at `surface.cpp:264` (`currentShadingState`) and `:304, 332, 334` (`displaceEstimate`), and a `CReyes *` passed where a `CShadingContext *` is expected at `curves.cpp:720`, `patches.cpp:1604, 1882`, `polygons.cpp:1314`, `subdivisionCreator.cpp:1868`, `subdivisionLoop.cpp:354` (`create(r)`) and `surface.cpp:452` (`classifyBound(r, ...)`); the core does the same in `render/geometryDispatch.cpp:101, 416, 455, 492, 587-600, 698, 733, 774`.
- **`CShadingContext` and `CReyes` are used by name outside Hiders and Shading**: 39 Geometry files (`intersect(CShadingContext *, CRay *)`, `dice(CReyes *)`, `threadMemory`, `displaceEstimate`, `currentShadingState`), 12 renderer-core files (`CRenderer::contexts` is a `CShadingContext **`, `renderer.cpp:819-845`; `dynamic_cast<CReyes *>(contexts[0])`, `:1029`; `CRenderer::activeContext`, `renderer.h:387`, set by `raytracer.cpp:420` and `reyes.cpp:229`, read by `imager.cpp:128`), 5 Texturing files (`lookup(..., CShadingContext *)`), and 49 test files.
- **Friends.** `CShadingContext` befriends `CPhotonHider`, `CProgrammableShaderInstance`, `CSphereLight` and `CQuadLight` (`shading.h:1175-1178`; the registry's `:901,903,904` are stale). The two lights call the protected hooks (`hcshader.cpp:120,135,435,450`); the photon hider overrides them and reaches no private member, and of the protected members it reads only `sampling` (`photon.cpp:211`, the object the public `samplingConfig()` returns) (confirmed and completed at plan, 2026-11-02: C4). `CProgrammableShaderInstance` is Shading's own and is not DB-020.
- **Registry numbers.** DB-001: 5 lines. DB-005: 8 include lines. DB-006: 31 lines (24 include, 7 link), one of them Shading-origin (`shading.cpp -> object.h`). DB-011: 168 lines (9 include, 159 link); Hiders' `CRenderer::` metric 558 references, 82 members (`metrics-baseline.txt:52`). DB-020: no allowlist lines (not a dependency edge). Shading's metric is already 0.

### User Story 1 - A static consumer launches with no workaround (Priority: P1)

A developer links their own program against `libRIBRender.a` and the shared `libshader_shading` through the CMake package, with no `-force_load`, `--whole-archive` or `--export-dynamic`. Today that program links and aborts at launch (`symbol not found in flat namespace '__ZTV12CTraceBundle'`, #24), unless the package injects the workaround. After this feature it launches and renders.

**Why this priority**: It is the acceptance of #35 and of the folded #24, and the user-visible consequence of the whole separation.

**Independent Test**: Build a minimal program that links `ribRender_static` the plain way (no force-load, no whole-archive) and renders one sphere with `Surface "matte"` through a `Display "file"` (the program of #24), on macOS and Linux; run it. Do the same through `find_package(openRender COMPONENTS Static)` from an installed tree.

**Acceptance Scenarios**:

1. **Given** the finished tree, **When** the plain static consumer is linked and run on macOS and on Linux, **Then** it exits 0 and writes the image, with no unresolved-symbol message at launch.
2. **Given** the CMake package, **When** its text is read, **Then** it contains no `-force_load`, `--whole-archive` or `--export-dynamic` for `RIBRender_static` and no comment that says a hider symbol is resolved at load time.
3. **Given** a program that loads `libshader_shading` with no `libRIBRender` anywhere in the process, **When** it starts, **Then** the library loads and its entry points are callable (a context with no hider is constructible, US4).

---

### User Story 2 - `libshader_shading` is self-contained (Priority: P1)

A maintainer inspects the shading library's link. It has no unresolved symbol, no flat-namespace import and no weakened undefined-symbol check on its consumers.

**Why this priority**: It is the acceptance line of #35, constitution Principle VIII rule 2 ("MUST NOT be done with unresolved symbols bound at load time"), and what lets Shading be tested, replaced and linked alone (rule 5).

**Independent Test**: Link the library with undefined symbols as an error on both platforms; inspect the result with `dyld_info -fixups` (macOS) and `nm -D --undefined-only` against the exports of `libRIBRender` (Linux); the standalone contract program of spec 026 runs without its load stub.

**Acceptance Scenarios**:

1. **Given** the finished tree on macOS, **When** `dyld_info -fixups libshader_shading.dylib` is read, **Then** no fixup is "dynamically looked up", and the link line carries no `-undefined dynamic_lookup`.
2. **Given** the finished tree on Linux, **When** `libshader_shading.so` is linked with `LINKER:--no-undefined`, **Then** the link succeeds, no consumer inherits `--allow-shlib-undefined` from it, and none of its undefined symbols is defined by `libRIBRender`.
3. **Given** the standalone contract programs of spec 026, **When** they are built, **Then** `tests/shading_contract/load_stub.cpp` is gone and they still pass.
4. **Given** a test that linked `ribRender` only to satisfy the library's imports, **When** it is relinked without it, **Then** it passes on both platforms (this includes `LibShader_McompConsistency`, #44).

---

### User Story 3 - Renders do not change (Priority: P1)

A user renders any scene, interpreter or JIT, any hider, any thread count. After this feature the images, diagnostics and determinism guarantees are the same.

**Why this priority**: The change touches every shading call, every trace and every hider. A restructuring that changes a pixel is a regression (specs 025, 026, 027 and 028 held the same line).

**Independent Test**: The visual, parity, `shading_parity`, `shading_contract`, `texture_tile`, determinism and architecture suites on master and on the finished tree, with the output bytes of the suite and of the determinism sweep compared on each platform.

**Acceptance Scenarios**:

1. **Given** every scene of the visual suite and the determinism sweep, **When** rendered on the finished tree with the same thread count and seed as on master, **Then** each output file is byte-identical to the baseline recorded on the starting tree, `.rslo` and `.slo` alike, on macOS and on Linux against its own baseline.
2. **Given** the trace-request replay of spec 026 (`tests/shading_contract/replay/{macos,linux}.txt`, recorded on the starting tree), **When** it runs on the finished tree, **Then** it passes with the recorded files unchanged.
3. **Given** the full test suite, **When** it runs on each platform, **Then** its pass/fail set equals the starting tree's, except the tests this spec turns green by removing their cause (SC-009).

---

### User Story 4 - Hiders own a shading context instead of being one (Priority: P2)

A contributor writing or changing a hider uses a Shading class that has no virtual functions and is owned by a Hiders base class. A contributor testing Shading creates a shading context without any hider.

**Why this priority**: It is DB-005 and the title of #35. It is P2 because the observable behaviour is the same; the P1 stories are what a user sees, and this one is what makes them true without a workaround.

**Independent Test**: A compile-time check that the shading context type is not polymorphic; a test that constructs one with no hider and runs a shader on it; the existing hider suites unchanged in output.

**Acceptance Scenarios**:

1. **Given** the finished tree, **When** `shading.h` is read, **Then** `CShadingContext` declares no virtual function and has no `friend` of another domain, and no class in Hiders derives from it.
2. **Given** the three hider families (REYES with its two rasterizers, raytrace, photon), **When** they are read, **Then** each is a hider that owns one shading context per thread and exposes it, with the main loop (`renderingLoop`) a virtual of the Hiders base and no longer of the Shading class.
3. **Given** the nine test classes that subclassed the shading context (five for an empty loop only, four also for the hooks; corrected at plan, 2026-11-02: C1), **When** they are edited (FR-025), **Then** they construct the context directly, and the four hook tests install a hook object.
4. **Given** the renderer core, **When** it creates the frame's contexts, **Then** it creates hiders, runs their loops, and hands the imager and the services the shading context through the hider's accessor; `CRenderer::contexts`, `activeContext`, `renderer.cpp:1029` and `reyes.cpp:1994` compile against the new types.

---

### User Story 5 - Shading reaches the hiders only through interfaces it owns (Priority: P2)

Shading no longer includes or links a Hiders type. The trace/transmission/gather bundles, the photon-map lookup and the illumination hooks reach Shading through interfaces it owns, implemented by the layers above.

**Why this priority**: It is DB-005's 8 include edges and DB-001's 4 imports; it is what makes US1 and US2 true.

**Independent Test**: The architecture check (`ctest -L architecture`) on both platforms; compiling `libshader_shading` without `src/ri/hiders` on its include path; the photon pass and the gather/transmission/trace scenes unchanged.

**Acceptance Scenarios**:

1. **Given** the finished tree, **When** the architecture check runs, **Then** it reports no Shading-to-Hiders include or link edge, and the 13 allowlist lines of DB-001 (5) and DB-005 (8) are deleted.
2. **Given** the shading library's include path, **When** it is read, **Then** it does not list `src/ri/hiders`.
3. **Given** `trace()`, `transmission()`, `visibility()`, `gather()`, `occlusion()`, `indirectdiffuse()` and the photon-map lookups, **When** they run under the interpreter and the JIT, **Then** the same rays are traced in the same order with the same sample identities (the spec 026 replay) and the same photon estimates are returned.
4. **Given** the photon pass and the built-in lights, **When** the illumination hooks are driven, **Then** the same hook calls happen with the same arguments (`ShadingParity_IlluminationHook` and its operand and condition tests), through a Shading-owned interface the photon hider installs on the context it owns.

---

### User Story 6 - The hiders are the tracers (Priority: P2)

The hider that owns a context answers the context's trace requests itself. `trace*` is no longer a member of the Shading class.

**Why this priority**: The second bullet of #35's scope. P2 because spec 026 already routed every Shading call through `ITracer`; this removes the adapter, the per-frame slot and the Shading class's trace members.

**Independent Test**: The spec 026 trace replay and tracer tests unchanged; a tracer double still receives the same requests; a grep shows no `trace`/`traceEx` member on the Shading class.

**Acceptance Scenarios**:

1. **Given** the finished tree, **When** `shading.h` is read, **Then** it declares no `trace`, `traceEx` or `traceAny`, and the trace bodies (`trace.cpp`) are Hiders members or functions that reach Shading's per-level state only through a documented public interface of the context (FR-017).
2. **Given** two hiders in the same process (a photon pass and a camera pass run as separate frames), **When** each traces, **Then** each uses its own tracer and none uses the other's (the per-frame slot of spec 026 has no reader left; FR-016).
3. **Given** a context with no hider and no tracer installed, **When** a traced builtin runs, **Then** it warns once per kind and treats the rays as misses, as spec 026 FR-012 requires.

---

### User Story 7 - `shade()` consumes `ShaderGlobals` (Priority: P3)

The input of `shade()` is the `ShaderGlobals` of spec 026 (its batch description: object and shader handles, ray type, hit mode, pass, path depth, transport, footprint), not a `CSurface *`. The ray type and hit mode are read from it.

**Why this priority**: The third bullet of #35's scope and the last Shading-to-Geometry include (`shading.cpp -> object.h`, DB-006). P3 because it carries the most design risk (the callbacks above), changes the most call sites and, unlike the rest, needed a maintainer decision (Q1, answered); it is ordered last so the P1/P2 stories can be reviewed first.

**Independent Test**: A producer double and the real producers fill `ShaderGlobals` for each row of the situation table; `ShadingContract_Situations` and the replay unchanged; the full visual suite byte-identical; the DB-006 line deleted.

**Acceptance Scenarios**:

1. **Given** each situation the renderer shades (rows 1-5, 8, 9 of the situation table; rows 6, 7, 10-14 are classify-only as in spec 026, B8), **When** the producer fills the batch, **Then** its `rayType`, `hitMode`, `transport` and `pass` equal `classifyShading()` (as corrected by this spec, D1) for that situation, including the oddities of #117, #118, #119 and #120, and `shade()` branches on them and on nothing else about the hider.
2. **Given** the finished tree, **When** `shading.cpp` and the Shading headers are read, **Then** none includes `object.h` or names `CSurface`/`CObject` other than as opaque, forward-declared types (the Geometry-facing forwarding entry `shade(CSurface *, ...)` passes its surface through without dereferencing it, until spec 031), and the `DB-006 ... shading.cpp -> object.h` line is deleted.
3. **Given** a grid, a span of ray hits with derivatives, a ray hit without, a displacement-only pass, a bound estimate, points and curves, **When** they are shaded, **Then** the values the shaders see and the images are byte-identical (the derivative-offset points are computed with the same expressions, FR-020).

---

### User Story 8 - The registry reflects what moved (Priority: P3)

A maintainer reading `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` and `tests/architecture/allowlist.txt` sees DB-005 and DB-001 under Resolved with this spec's number and date, DB-020 narrowed to its non-Shading parts, DB-006 and DB-011 reduced, and the allowlist smaller by exactly the edges that went.

**Why this priority**: Required by constitution rule 8 and by #35's acceptance; mechanical once the code has moved.

**Independent Test**: `ctest --test-dir build -L architecture` on both platforms; diff the allowlist and the registry against the starting tree.

**Acceptance Scenarios**:

1. **Given** the finished tree, **When** the architecture check runs, **Then** it passes with no `VIOLATION`, `STALE` or `BADID` line and no allowlist line was added or re-keyed (FR-028).
2. **Given** DB-001 and DB-005, **When** the registry is read, **Then** each is under Resolved with "spec 029" and the date, and no text refers to anything that exists only after the merge.

---

### Edge Cases

- **No services and no hider**: a context built with no hider and no services answers every routed lookup as specs 024/026 define (warn once per kind, not-found or miss values), and `newState()` keeps its documented refusal; nothing dereferences a missing hook, tracer or photon map.
- **Re-entrancy**: a tracer that shades what it hits re-enters `shade()` on the same context in a new shading state one ray level deeper, as `traceEx` does today; the level count, the ray label and the free-state list survive the move out of the Shading class (FR-017).
- **Concurrency**: every render thread has its own hider and its own context; the process-wide state that remains (services, the default-services pointer) is unchanged. The photon pass and a camera pass are separate frames and never share a context; the plan restates why no two contexts share a tracer.
- **The imager** shades with no object (`rendererDisplay.cpp:315`, `imager.cpp:128`): it takes the calling thread's context through the hider; its behaviour is unchanged.
- **Builds without the JIT** keep building and passing; the `.slo` halves of the tests register only with the JIT on. Builds without OpenEXR are unaffected.
- **`ribVector` / `ribOut` builds** (no rendering pipeline; `geometryDispatchStub.cpp`, `irradianceDispatchStub.cpp`) still link; they never contained a hider or a shading context.
- **Debug builds** keep their asserts, including `assert(freeStates != NULL)` (`trace.cpp:263`) and the vertex-count asserts in `shade()` (`shading.cpp:866`).
- **`libshader_jitmeta`** (the bitcode-metadata split for `sloinfo`) is left as it is.
- **Plug-in boundaries**: shadeops, DLO/implicit modules and display drivers are untouched; the public C ABIs of spec 037 do not change.

## Requirements *(mandatory)*

### Functional Requirements

**The link (US1, US2)**

- **FR-001**: `libshader_shading` MUST link with undefined symbols as an error on macOS and Linux and MUST have zero flat-namespace imports. On macOS this is the removal of `-undefined dynamic_lookup` (`CMakeLists.txt:85`) with ld64's default in force (the explicit `-undefined error` spelling is deprecated, spec 023 research); on Linux `LINKER:--no-undefined` on the library, as `src/common/CMakeLists.txt:80` does for `libRIBCommon`. The verification is by `dyld_info -fixups` on macOS and by a no-undefined link plus `nm -D --undefined-only` on Linux, recorded.
- **FR-002**: The INTERFACE option `-Wl,--allow-shlib-undefined` (`CMakeLists.txt:100`) MUST be removed, so no consumer's undefined-symbol check is weakened. The stale comments (`CMakeLists.txt` header `:11-14`, `:82-84`, `:87-98`, `:103-105`; `tests/shading_parity/CMakeLists.txt:316-389`; `src/libshader/tests/CMakeLists.txt:88-98`) (line numbers corrected at plan, 2026-11-02: C2, C3) MUST be corrected in the same change.
- **FR-003**: The `-force_load` / `--whole-archive` / `--export-dynamic` block and its comment in `CMake/openRenderConfig.cmake.in:79-97` MUST be removed, and so MUST the copy in `tests/unit/common/CMakeLists.txt:77-84`, whose test becomes a plain link (it must keep checking the shared diagnostic sink across libraries). `INSTALL_ARTIFACTS.md:89`, `DEVNOTES_DETAILS/OSHADER_UPDATES.md:192` (re-read; the `libshader_jitmeta` reason may stand) and DB-001's "Consequences today" MUST stop describing the workaround as required.
- **FR-004**: A static consumer test MUST exist and be green on macOS and Linux: a program linked plainly against `ribRender_static` (and, through the installed package, `openRender::RIBRender_static`) with no force-load, whole-archive or export-dynamic, that renders the #24 scene (`RiBegin`, `Display "file"`, `Surface "matte"`, one `Sphere`) and exits 0. It MUST be written first and shown failing on the starting tree on macOS (the launch abort of #24) and on Linux if the starting tree fails there. The installed-package half runs as the opt-in `Link_PackageStaticConsumer` in Phase 8 (SC-002, Q14 (a)), its red evidence taken in Phase 1. *(Amended 2026-11-02.)*
- **FR-005**: `tests/shading_contract/load_stub.cpp` and its CMake wiring MUST be deleted, and the standalone programs MUST pass without it. Tests that link `ribRender` only to satisfy the library's imports, and the `--no-as-needed` workarounds attached to them (GitHub #44), MUST be relinked without it where their purpose allows. Nothing may be disabled or loosened to get a green result.
- **FR-006**: The include path of `libshader_shading` MUST NOT list `src/ri/hiders`; it MUST NOT list `src/ri/geometry` if no Shading source includes a Geometry header after US7. The plan lists any other entry that goes (a DB-023 reduction; reaching zero is not required).

**Composition (US4)**

- **FR-007**: `CShadingContext` MUST declare no virtual function (including its destructor) and MUST be constructible without a hider. The pure virtual `renderingLoop` MUST move to a Hiders base class (`CHider`) that owns the shading context. No class in Hiders MAY derive from `CShadingContext`; the REYES hierarchy (`CReyes` and its two rasterizers `CStochastic`/`CZbuffer`), `CRaytracer` and `CPhotonHider` MUST be hiders. Each hider owns one context per render thread, as today.
- **FR-008**: The `friend` declarations of `CPhotonHider`, `CSphereLight` and `CQuadLight` in `shading.h` MUST be removed, with the members they needed made available through public, documented, Shading-owned interfaces (the illumination hooks, FR-013, and the ray-level scope, FR-017). `CProgrammableShaderInstance` (Shading's own) MAY stay.
- **FR-009**: The renderer core's use of the context MUST follow: `CRenderer::contexts`, `activeContext`, `dynamic_cast<CReyes *>(contexts[0])` (`renderer.cpp:1029`), the dispatch (`:1052`), `reyes.cpp:1994`, `imager.cpp:128`, `rendererJobs.cpp:88`, `rendererDeclarationsDispatch.cpp:82`. The thread-local `activeContext` MAY be set by the core's dispatcher instead of by each hider's loop (that would remove the DB-011 lines for it; the plan measures).
- **FR-010**: Geometry call sites that name `CReyes` or `CShadingContext` MAY be edited minimally to compile against the composed types (an accessor instead of an inherited member); the measured sites are listed in the starting point (C5). No Geometry interface changes shape: `IDicer`, `IRayContext`, the Dispatch/Stub pairs and every other DB-006/DB-004 edge of Geometry stay spec 031's.

**Interfaces Shading owns (US5)**

- **FR-011**: Shading MUST construct and run the trace, transmission and gather bundles without including or linking any Hiders type. The bundle classes (`CTraceRay`, `CTraceBundle`, `CTransmissionRay`, `CTransmissionBundle`, `CGatherRay`, `CGatherBundle`) contain only Shading logic and depend only on `CRay`/`CRayBundle`, `shaderPl.h`, diagnostics and statistics, so they are expected to be owned by Shading, with Hiders' users (`irradiance.cpp`, `irradianceDispatch.cpp`) and the core's three includes switched to the new header in the same change. The plan decides the mechanism; the requirement is the result (FR-014's edges gone).
- **FR-012**: Shading's photon-map lookups (`giFunctions.h:298,344`, `jitPhotonMap`) MUST go through a lookup interface owned by Shading and implemented by the photon map, with the photon map's storage and file handling staying in Hiders (spec 032 owns the wider GI split). The route is Q2 (answered, option A): `CRendererServices::getPhotonMap` returns the Shading-owned interface and `CPhotonMap` implements it. After it, no Shading source includes `photonMap.h`, and `CPhotonMap::lookup` is not imported.
- **FR-013**: The illumination hooks (`solarBegin`, `solarEnd`, `illuminateBegin`, `illuminateEnd`) MUST be a Shading-owned interface installed on the context by the hider that wants them (the photon hider) and reachable by the built-in lights through a public accessor. The `rendererIlluminationHook()` query, the interpreter macros and the `jit*Begin`/`End` functions keep calling the hooks at the same places with the same arguments (`shaderOpcodes.h:216,272,382,421`, `shading.cpp:2148-2240`); `CHookProbeContext`-style tests install a hook object instead of overriding virtuals.
- **FR-014**: The Shading-to-Hiders include edges MUST all go: the 4 `bundles.h`, the 2 `irradiance.h` (a build confirms they are unused, otherwise the plan says what they provide) and the 2 `photonMap.h` lines of DB-005, and DB-001's 5 link lines. No new include or link edge from Shading to a higher domain, and none from Geometry to Shading or Hiders (FR-022, FR-028), may appear.
- **FR-015**: Any change to `CRendererServices`, `ITracer` or `ShaderGlobals` MUST be recorded as a new `kRendererServicesVersion` with its history entry in the header. Under the maintainer's policy (Session 2026-11-02) MAJOR stays at 2 until spec 055: spec 029 takes the version from 2.0.0 to **2.1.0**, and the history comment MUST say that 2.x is unstable until the milestone closes (spec 055), so a MINOR step before then may break; the 2.1.0 entry names each change that would be MAJOR after the milestone (the `getPhotonMap` result type, `tracer()`'s meaning, `BatchShape::Span`'s meaning). Every implementer's `static_assert` stays on MAJOR 2. *(Amended 2026-11-02.)*

**Tracing (US6)**

- **FR-016**: `trace(CRayBundle *)`, `traceEx(CRayBundle *)`, `trace(CRay *)` and the unused `traceAny` declaration MUST leave the Shading class. The hiders MUST implement `ITracer` themselves, each for the context it owns, so the stateless `CHiderTracer` adapter (`hiderTracer.{h,cpp}`) and the per-frame slot `CRenderer::frameTracer` of spec 026 are removed unless the plan shows a reader that still needs one. Every request Shading issues (the spec 026 replay, 16 sections, both backends) MUST arrive at the hider in the same order with the same rays. `libshader::traceRay`/`traceBundle` keep warning once per kind and treating rays as misses when no tracer is installed.
- **FR-017**: Moving `traceEx` out of the class MUST NOT widen the context's private surface ad hoc. Entering and leaving a ray level (the depth rule against the maximum ray depth, the ray label, the free-state list, `newState()`/`freeState()`) and the per-thread object hash MUST be a documented public interface of the context, or state the hider owns, and the depth rule MUST remain applied exactly once. `CRay`'s and `CRayBundle`'s protocol (`postTraceAction`, `postShade`, `post`) is unchanged.

**`shade()` and `ShaderGlobals` (US7)**

- **FR-018**: The shading entry `shade()` MUST take the `ShaderGlobals` description plus the Shading-owned point-source interface of FR-021 instead of `CSurface *`, and Shading MUST NOT include `object.h` or call a `CSurface` method. Geometry's calls (`dlobject.cpp:173`, `implicitSurface.cpp:242`, and `displace()`/`displaceEstimate()` from `surface.cpp`) keep their unchanged `shade(CSurface *, int, int, EShadingDim, unsigned int, int)` symbol: it stays in Shading as an opaque forwarding entry that never dereferences the surface and hands it to the owning hider's Shading-owned `IShadingProducer`, which builds the point source and the batch; with no producer installed it warns once and shades nothing. Spec 031 retires the entry. *(Amended 2026-11-02, Q1 (a): this keeps FR-010, FR-021, FR-022 and FR-028 true together.)* The explicit ray type and hit mode MUST be the only inputs `shade()` uses to choose the hit-mode branches (`shading.cpp:886-912`, the camera-hit-mode overwrite after the shaders) and the atmosphere rule (`currentRayDepth == 0`), reproducing the situation table row by row.
- **FR-019**: Producers MUST fill the batch per the presence rules of spec 026 (`data-model.md`). A producer MUST derive `rayType`/`hitMode`/`transport`/`pass` from the same inputs as `classifyShading()` (dimension, `displaceOnly`, `inShadow`, ray depth, the illumination hook, the frame's hider kind, the three hit-mode attributes). A divergence between the producer and `classifyShading()` for any suite scene MUST fail a test. `classifyShading()` is corrected to the code first (D1, Q5 (a)): a shadow hit reads `transmissionhitmode` at every dimension; a 2-D or 0-D ray hit at any depth, raster frames included, is `Camera`/`Secondary` (2-D reads `specularhitmode`); only the photon-emission batch (hook on, 0-D, depth 0, not in a shadow) is `Photon`; `ShadingContract_Situations` is edited to the corrected table. *(Amended 2026-11-02.)*
- **FR-020**: The derivative-offset construction (the two extra points per ray hit, their `du`/`dv`, `time`, `I`) and the grid `du`/`dv` formula MUST keep their arithmetic expressions, operand order and precision wherever they move (gotcha 26: another grouping or width changes the last bit, and Clang and GCC contract multiply-adds differently).
- **FR-021**: Primitive-variable interpolation into the shader locals and the second sampling of the derivative path are inputs `shade()` cannot take before it has chosen and prepared the shaders. They MUST therefore reach Shading through a Shading-owned point-source interface (Q1, option B) with two operations: re-sample a range of the batch's points, and interpolate the primitive variables into the shader locals. A Hiders-side adapter over the unchanged `CSurface::sample()`/`interpolate()` implements it, so no Geometry source changes and Geometry gains no edge. Shading keeps the derivative-offset arithmetic and the call order exactly as today (FR-020). The interface is part of the versioned boundary (FR-015).
- **FR-022**: Geometry MUST NOT gain an include of any Shading or Hiders header, and Shading MUST NOT gain one of any Geometry header. The producer and the point-source adapter live in Hiders (Q1), which may see both domains, so that holds.

**Behaviour and tests**

- **FR-023**: Rendering output MUST NOT change: byte-identical on macOS for every suite scene and every determinism-sweep scene, `.rslo` and `.slo` alike, and byte-identical on Linux against the Linux baseline taken on the starting tree (Phase 1 records both, as spec 026 did). A difference stops the phase for the maintainer; no tolerance is widened, no test is disabled (constitution III).
- **FR-024**: Tests MUST be written first and shown failing on the starting tree: the link checks (FR-001, FR-002 on each platform), the static consumer (FR-004), the "no virtuals / constructible alone" check (FR-007), the interface checks of FR-011 to FR-013, the tracer-per-hider check (FR-016), the producer-versus-`classifyShading()` check (FR-019), and the grep-style scope checks (no `friend`, no Shading include of a Hiders or Geometry header, no workaround text in the package).
- **FR-025**: Existing tests MUST keep passing without edits beyond: the base class and constructor of the nine context-subclassing test classes (FR-007; corrected at plan, 2026-11-02: C1) and the hook overrides of the four hook-probe classes (FR-013); include paths; the doubles' implementations of any member FR-015 changes (the six `getPhotonMap` overrides); the removals of FR-005; the test writes of the shaded object, which become `setShadedObject()` calls (`contract_harness.h:103`, `rendered_harness.h:104`, `case_offending_object.cpp:50, 59`, `case_type_mismatch_real.cpp:87`; Q3); `ShadingContract_Situations`'s expected rows and two appended scenes (`case_situations.cpp:177-212, 273-280, 313-323`; Q5); and `test_state_services.cpp:64-70`'s read of the vertex memory, which uses the context's `vertexMemoryInUse()` (Q11); the mechanical `->shadingContext()` edit of the 11 reads of `CRenderer::contexts[...]` that treat it as a `CShadingContext *` once it is a `CHider **` (Q6 (a); `csgTestUtils.h:157`, `test_sampling_option_rsl.cpp:47`, `scene_probe.h:133`, `replay_capture.h:165`, `test_frame_params_equal_core.cpp:131`, `test_ambient_accumulation.cpp:154, 235`, `test_derivative_inactive_lanes.cpp:171`, `test_derivative_inactive_grid_lanes.cpp:243`, `test_state_services.cpp:216-217`; no new `CRenderer` accessor; spec correction C8); four further shaded-object writes that become `setShadedObject()` calls (`texture_substitute_harness.h:89`, `test_ambient_accumulation.cpp:175, 255`, `test_false_if_constructs.cpp:99`; spec correction C7); and, only if the build shows a test lost a transitive include when Shading stopped including `object.h`, the include path of `scene_probe.h:74-75` and `test_displace_consistency.cpp:55`. Under Q2 (a) no test changes how it installs a tracer. *(Amended 2026-11-02; extended at the tasks stage, 2026-11-02, with the maintainer's approval.)* Each edit is named at the TEST-APPROVAL of its phase. The recorded replay files and the baseline manifests are never edited.
- **FR-026**: Performance: no scene's render time may regress beyond the run-to-run noise measured on the starting tree (the spec 026 SC-010 method). The hot paths touched are the per-point `shade()` entry and the trace calls.
- **FR-027**: Linux (GCC 13 `-Werror`) MUST be validated over ssh in a per-spec tree with the remote `cpujob` at `-j2`, with the Linux baseline taken before the first change (the method and gate of spec 026, plan Q13).

**Registry and guardrails**

- **FR-028**: The architecture check MUST pass on macOS and Linux. The allowlist MUST only shrink: every line whose edge goes is deleted, and none is added or re-keyed. DB-006's link lines name `CShadingContext::shade(CSurface*,...)`, `saveState()` and `restoreState(void*)` as called from Geometry; a change that renames or re-signs one while Geometry still calls it would stale the line and need a new one. The change MUST therefore either keep Geometry's call compiling against an unchanged symbol, or delete the line by moving the call; re-keying is allowed only with the maintainer's explicit sign-off recorded in the plan. The plan lists each line expected to go from a measurement.
- **FR-029**: `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` MUST be updated: DB-005 and DB-001 move to Resolved with "spec 029" and the date and the measured end state; DB-020's evidence drops the Shading friends and stays Open for the irradiance, texture3d, point-cloud and brick-map friends (specs 030, 032), with its tracking updated; DB-006's counts and history record the deleted `shading.cpp -> object.h` line (31 -> 30) and anything else measured; DB-011's history records any deleted line and the metric; DB-016's sentence that the arena and stats injection "stays with spec 029" is corrected to name no spec and to cite #135 (Q3, option A). No text, task or success criterion may depend on the merge.
- **FR-030**: `tests/architecture/metrics-baseline.txt` MUST be lowered to the measured end values if any reference goes, and no class's count may rise.
- **FR-031**: The stale line references in the registry (DB-005's `reyes.h:104`, `raytracer.h:140`, `trace.cpp:47,240,289`; DB-020's `shading.h:901,903,904`; DB-013's `shading.h:903-904`) MUST be corrected to the current ones in the same edit.
- **FR-032**: No task or text MAY depend on the merge. Issues close through the maintainer's merge message (`Closes #35`, `Closes #24`); this spec does not write it. CLAUDE.md changes (gotcha 28's load-stub sentence, a new gotcha, the Active Technologies and Recent Changes entries) are proposed as a draft file in the spec directory at the end and applied only by the maintainer.

### Key Entities

- **Shading context** (`CShadingContext`): the per-thread state and execution of shading: no virtuals, no friends, constructible alone, owned by a hider.
- **Hider** (`CHider`, new in Hiders): the base of the three hider families; owns the shading context, defines the main loop, and is the `ITracer` for its context.
- **`ITracer`** (spec 026): the Shading-owned interface for tracing; implemented by the hiders themselves after this spec.
- **Illumination hook interface** (new, Shading-owned): the four photon-pass hook calls, installed on a context by the hider.
- **Photon lookup interface** (new, Shading-owned; Q2): what Shading asks of a photon map; `CPhotonMap` implements it and `getPhotonMap` returns it.
- **Ray bundles** (`CTraceBundle`, `CTransmissionBundle`, `CGatherBundle` and their rays): Shading-side post-processing of traced rays, owned by Shading after this spec.
- **`ShaderGlobals` and the point-source interface** (spec 026 plus Q1): the input of `shade()`: the batch description, and a Shading-owned interface for re-sampling a range and interpolating primitive variables, implemented in Hiders over `CSurface`.
- **Static consumer**: a program linked plainly against `libRIBRender.a` and the shared `libshader_shading`.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On macOS `dyld_info -fixups` of `libshader_shading` lists 0 dynamically-looked-up symbols (4 before), and the library links with undefined symbols as an error; on Linux it links with `--no-undefined` and none of its undefined symbols is defined by `libRIBRender` (4 before), with no INTERFACE `--allow-shlib-undefined` left (US2, FR-001, FR-002).
- **SC-002**: A static consumer linked plainly (no force-load, whole-archive or export-dynamic), both against the build tree and through `find_package(openRender COMPONENTS Static)` from an installed tree, launches and renders the #24 scene on macOS and Linux (US1, FR-004). The build-tree consumer (`Link_StaticConsumer`) runs at every checkpoint; the installed-tree consumer (`Link_PackageStaticConsumer`, label `link-package`, opt-in) runs once per platform in Phase 8, from a scratch configure whose install prefix is the scratch prefix, so GitHub #23 (`cmake --install --prefix` broken in self-contained builds) is not hit; its red evidence is taken in Phase 1. *(Amended 2026-11-02, Q14 (a).)*
- **SC-003**: A search of `CMake`, `src/libshader` and `tests/unit/common` for `force_load`, `whole-archive`, `export-dynamic`, `allow-shlib-undefined` and `dynamic_lookup` finds no use for the shading library or the package (the `tests/plugins/dynamicload` use of `dynamic_lookup` for plug-in test modules is unrelated and stays), and `load_stub.cpp` no longer exists (FR-003, FR-005).
- **SC-004**: `CShadingContext` is not polymorphic and has no `friend` of another domain (a compile-time check and a grep), and a test constructs one with no hider and runs a shader on it under both backends (US4, FR-007, FR-008).
- **SC-005**: The architecture check passes on macOS and Linux with DB-001's 5 lines and DB-005's 8 lines deleted and DB-006's `shading.cpp -> object.h` line deleted (31 -> 30), no line added or re-keyed, DB-011 not up, and `libshader_shading` compiles with `src/ri/hiders` off its include path (US5, US7, US8, FR-006, FR-014, FR-028).
- **SC-006**: Every scene of the visual suite and of the determinism sweep renders to bytes identical to the baseline recorded on the starting tree, on macOS and on Linux; the spec 026 trace replay passes with its recorded files unchanged; every `MaxBlockAvgDiff` equals the baseline (US3, FR-023).
- **SC-007**: No hider derives from `CShadingContext`; `shading.h` declares no `trace`, `traceEx` or `traceAny`; the per-frame tracer slot and its adapter are gone or the plan names the reader that keeps one; each hider is the tracer of the context it owns (US6, FR-016, FR-017).
- **SC-008**: For each row of the situation table that a scene reaches, the `ShaderGlobals` the producer fills has the ray type, hit mode, transport and pass that `classifyShading()`, as corrected by this spec (D1), returns; no Shading or Hiders header is included by Geometry and no Geometry header by Shading (US7, FR-018, FR-019, FR-022).
- **SC-009**: The full suite on Linux has the starting tree's pass/fail set except the tests this spec turns green by removing their cause; `LibShader_McompConsistency` (#44) is expected to be one, and each change in the set is explained in the validation record (FR-005).
- **SC-010**: A shading-heavy scene's render time on the finished tree is within the run-to-run noise measured on the starting tree on the same machine and thread count (FR-026).
- **SC-011**: `DOMAIN_BOUNDARIES.md` shows DB-005 and DB-001 under Resolved with "spec 029" and the date, DB-020 narrowed, DB-006 and DB-011 histories updated, the stale line numbers corrected, and no entry refers to the merge; the metrics baseline is current (US8, FR-029 to FR-031).

## Scope Boundaries

**In scope (029)**: the composition of hiders over a virtual-free, friend-free shading context; the hiders as tracers and the removal of `trace*` from Shading; the Shading-owned interfaces for bundles, photon lookup and illumination hooks; `shade()` consuming `ShaderGlobals`; removal of the link wiring, the package workaround and the load stub; the static-consumer test; the registry and allowlist edits; the boundary's version step to 2.1.0 (FR-015).

**Out of scope, and whose it is**:

| Item | Placement | Reason |
|---|---|---|
| #117 `camerahitmode` honoured only for REYES grids | out (a later spec; after 029 a one-place fix) | changes images; FR-023 |
| #118 `diffusehitmode` parsed, never read | out | changes images; the parser is Scene Interface |
| #119 implicit-surface hits shaded 0-D | out, spec 031 | a Geometry source, an image change |
| #120 three behaviours to verify against the RISpec | out; stays a verification issue | not a code change here |
| #123 light-parameter default in a named space | closed; nothing to do | fixed on `fix/123-named-space-light-default` |
| #134 bind-time coordinate systems bypass `getMatrix` | out, spec 033 | shader binding is 033 (DB-007); 029 does not touch `init.cpp`'s lookup |
| #44 Linux `LibShader_McompConsistency` cannot load | in as a consequence (FR-005, SC-009) | self-containment removes its cause |
| Hider registry, `strcmp` chains, hcshader retirement (DB-013) | spec 030 | 029 keeps the `strcmp` chain and `hcshader` |
| `IDicer`/`IRayContext`, Dispatch/Stub pairs, other Geometry edges (DB-004, DB-006, DB-017) | spec 031 | 029 only edits call sites to compile (FR-010) |
| GI cache split, irradiance/photon-map storage, DB-010, DB-025, the other DB-020 friends | spec 032 | 029 adds a lookup interface only (FR-012) |
| `ShadingSystem` / `IShaderBackend`, the JIT's message-passing entry points | spec 033 | |
| Shader networks, `setmessage` | spec 034 | |
| The OSL backend, the public runtime library | specs 049, 052 | 029 shapes `ShaderGlobals` so they can adapt it |
| Shading-rate unification | spec 054 | the footprint stays today's value |
| Arena and statistics injection as per-context state (DB-016 follow-up) | out, #135 | not in #35's scope; Q3 |
| Retiring `libshader_jitmeta` | not needed | its reason stands (Edge Cases) |

## Assumptions

- Specs 023, 024, 025, 026, 027, 028, 037 and 039 are merged on master (`d6910bd0`); the base of this spec is master and no rebase is assumed.
- `ITracer`, `ShaderGlobals`, `classifyShading()`, `CRendererServices` 2.0.0 and the 026 test doubles and replay exist as 026 left them; this spec evolves them under FR-015 and does not re-open spec 026's decisions (its plan Q1-Q14, the tasks-stage QA-QG and the analyze-stage B1-B8).
- Hiders may depend on Shading and on Geometry (the allowed-dependency table), so `CHider` owning a `CShadingContext` and implementing `ITracer` adds no forbidden edge. Shading may not depend on Hiders or Geometry, and Geometry not on Shading or Hiders; the producer of `ShaderGlobals` (Q1) and the placement of every new header are chosen with that table in hand, and the architecture check is the judge.
- The bundle classes need no Hiders type, as a read of `bundles.{h,cpp}` shows, so moving them to Shading is a relocation and not a redesign; the plan confirms by compiling.
- `CPhotonHider` reaches no private member of `CShadingContext` (confirmed by the plan's inventory; of the protected members it reads `sampling`, C4); Phase 5 confirms by building without the friend.
- The renderer core stays the composition root: it creates the hiders (`renderer.cpp:819-845`, the `strcmp` chain), installs the services and runs the loops.
- Linux is validated by the agent over ssh (`juvenal@ubuntu-2404-desktop.v2-labs.io`) in a per-spec tree with the remote `cpujob` at `-j2`, as in spec 026; macOS builds use `~/.claude/bin/cpujob` at `-j4` at most.
- The determinism sweep (`-L '^determinism-sweep$' -j2`, 419 scenes) is run on both platforms before the end, not on every phase (the method of spec 026, B1).
- No Windows support is assumed or removed (existing Windows code paths are left alone unless a change subsumes one).

## Open Questions

None remain; all three were answered on 2026-11-01 (see Clarifications, Session 2026-11-01).
