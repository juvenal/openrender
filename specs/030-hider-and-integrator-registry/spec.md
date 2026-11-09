# Feature Specification: Hider and Integrator Registry

**Feature Branch**: `030-hider-and-integrator-registry`
**Created**: 2026-11-06
**Status**: Approved (plan)
**Base**: master `e83d005d` (spec 029 merged)
**Input**: Constitution Principle VIII remediation, layer L4, after spec 029 (merged); GitHub #36. Hiders stop being chosen by `strcmp` chains in the renderer core and register themselves (name, aliases, declared capabilities); an **integrator** registry is built the same way, with `RiIntegrator` and the RIB `Integrator` statement carrying `"classic"` (today's behaviour) as the only implementation and the default; hiders declare what they provide and integrators what they require, and an unsupported pair is refused with an explicit diagnostic; the constitution gains a tenth domain, **Transport** (decision D30), for integrators; the built-in `hcshader` lights (`CSphereLight`, `CQuadLight`) retire. Resolves registry entry DB-013 and reduces DB-011. Renders stay byte-identical.

## Clarifications

### Session 2026-11-06 (specify stage, before maintainer review)

- The authoritative scope is the **plan's** brief for spec 030 (`there-is-an-issue-glowing-perlis.md`, the 030 brief and its table row), not the narrower body of issue #36. The issue lists the hider factory and the `hcshader` removal only; the integrator registry, `RiIntegrator`/`Integrator`, capability negotiation and the D30 amendment come from the plan (decisions D1, D2, D8, D21, D28, D30 of `DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md`). Section "Source reconciliation" lists every difference found between the plan, the issue and the code.
- Everything not asked of the maintainer is decided here as an assumption the maintainer can overrule cheaply (Assumptions).

### Session 2026-11-06 (clarify stage, answered by the maintainer)

- Q: What does `classic` own in this spec (FR-016)? -> A: `classic` is a registered name with its declared requirements and a pass-through lifecycle (`configure` / `prepare` / `finish`) that the core really calls at frame begin and end. Today's transport call sites (`illuminance`, `trace`, `gather`, `indirectdiffuse`, the photon hook) stay where they are (they are Shading builtins and Transport may depend on Shading, never the reverse). The lifecycle slots are pinned in the interface's version history so that spec 042 extends them (render work, services) by MINOR steps instead of reshaping them.
- Q: Form of the `Integrator` statement (FR-020)? -> A: `Integrator "name" [parameters...]`, stored in `COptions` beside `hider`. The name `handle` is reserved as a string parameter name, so a RIS-style handle can be added later without a grammar change. Parameters are an opaque typed name/value list; names the Scene Interface does not pre-declare use inline declarations (`"int maxdepth" [5]`), and the integrator validates them at frame begin.
- Q: What does "fails" mean for an unknown integrator and for an unsupported pair (FR-024)? -> A: Split. An unknown integrator name reports an error and falls back to `classic` (as an unknown hider falls back to reyes; `orender` still exits -1 because `RiLastError` is set). An unsupported (hider, integrator) pair is refused: the frame is not rendered. Check order: resolve the hider with its fallback, resolve the integrator with its fallback, then check the pair on the resolved ones. How a refused frame passes through `RiWorldEnd` is for the plan; if it proves too invasive, the plan stops and asks the maintainer.
- Q: How much of a hider is behind the C function table (FR-011)? -> A: Registration level only. A versioned table starting with `OrTableHeader` carries name, aliases, capabilities, create/destroy with an opaque handle, pre-display setup and dispatch kind. The hider's runtime stays internal C++, documented as not external until spec 051.
- Q: Retiring `hcshader` (FR-026)? -> A: Accepted by the maintainer. A named `spherelight`/`quadlight` with no compiled shader on the path reports `Failed to find shader`; NEWS records it and the legacy page is fixed (FR-028). With the shipped shaders on the path the compiled `.sl` is found first, so no image changes.
- Q: `reyes.cpp -> CRenderer::contexts` (FR-008)? -> A: A MUST. The `reyes.cpp -> CRenderer::numThreads` line (`allowlist.txt:176`) is optional (deleted if the same peer-group mechanism retires it).
- Q: Capability vocabulary (FR-009)? -> A: One vocabulary of capabilities identified by string name plus version, covering both what integrators need and the hiders' static properties the core reads; absence of a capability means the hider is limited in that respect. `HIDER_GEOMETRYHOOK` (defined, never used) does not become a capability. The registration shape carries an optional integrator field so an alias can later stand for a pair; precedence of such an alias against an explicit `Integrator` is deferred to spec 050. The other listed assumptions stand.

### 2026-11-07 (plan-stage corrections, maintainer-approved plan)

The plan (`research.md` R12's "Spec reconciliation" bullets, numbered R16-R19 like the Source reconciliation rows below) found places where this spec did not match the code; they are corrected here without changing any requirement's intent, and FR/SC numbering is unchanged:

- `rendererStatics.cpp` includes five concrete hider headers (`photon.h`, `raytracer.h`, `reyes.h`, `stochastic.h`, `zbuffer.h`) plus `hcshader.h`, not "none": Measured starting point, FR-025 (the `hcshader.h` include of `rendererContext.cpp:49` added to its list) and Source reconciliation R13 corrected; R16 added.
- `Hider` has no `RiGetOption`-style query (`COptions::find` answers only the hider *parameters*), so `Integrator` gets none: FR-017 no longer asks for a `find()` entry or a query, and its pre-declaration layer now defers to FR-020 (no pre-declaration; inline declarations). R19 added.
- `CReyes::drawObject` already exists (`reyes.h:247`): FR-007 now names the `rasterization` capability and a virtual `drawObject` on the `CHider` base that the existing `CReyes::drawObject` overrides. R18 added.
- Static-link proof is a new test, `Link_StaticConsumerRegistries`, built like `Link_StaticConsumer`, which stays untouched (maintainer answer Q7): US2 AS3 and FR-006 reworded.
- Recorded with the plan's other answers (no requirement change): `HIDER_PHOTONMAP_OVERWRITE`, set at `photon.cpp:118` and read nowhere, is deleted with no replacement capability (FR-008, maintainer answer Q3); R17 added.

### 2026-11-09 (implement-stage amendment F1, maintainer-approved)

The Phase 1 measurement (tasks.md T017 (a)) found that the plan's claim "no test names `hiderFlags` or `HIDER_*`" was wrong: four approved `tests/shading_parity` tests (`test_false_if_constructs.cpp`, `test_illumination_hook.cpp`, `test_illumination_hook_conditions.cpp`, `test_illumination_hook_operands.cpp`) switch the illumination hook with `CRenderer::hiderFlags |= / &= HIDER_ILLUMINATIONHOOK`. FR-008 (delete the flag, read the capability from the registration) therefore needs a test edit. Maintainer decision: a pinned-test edit with a diff-first pause (tasks.md T109a); only the hook-switching lines and their save/restore move to the frame traits (`frameTransport().traits.illuminationHook`, through a test-only setter if one is needed), every assertion unchanged. No requirement changes; see Source reconciliation R20.

## User Scenarios & Testing *(mandatory)*

The "users" are: scene authors and pipeline TDs who write RIB and call the RI API (and must see no change); contributors who add a hider or an integrator (specs 042, 050, a future volume hider or debug hider); maintainers reviewing Principle VIII remediation; and the authors of the specs that build on this one (031 geometry hider-agnostic, 032 GI cache split, 041 light service, 042 direct integrator, 050 path-tracer integrator, 051 run-time plug-in loading, 056 volume shaders, 057 area-light geometry).

### Measured starting point (2026-11-06, master `e83d005d`)

Method: static reading of the tree in this worktree and of the spec 022 registry; no build or test was run for the specify stage. Line numbers are those of `e83d005d`; the plan's and the registry's predate spec 029 and several are stale (Source reconciliation).

- **Two `strcmp(hider, ...)` chains in the renderer core.** `CRenderer::beginFrame()` (`src/ri/render/renderer.cpp:778-792`) picks the hider's static `preDisplaySetup()` by name (`raytrace`, `reyes`, `zbuffer`, `photon`; anything else logs `Hider "%s" unavailable` with `CODE_BADTOKEN` and runs `CStochastic::preDisplaySetup()`), and `:815-834` constructs one hider per render thread and picks the job dispatcher (`dispatchReyes` for `raytrace`, `reyes`, `zbuffer` and for the unknown-name fallback, which constructs a `CStochastic`; `dispatchPhoton` for `photon`).
- **The alias and its siblings.** `CRendererContext::RiHiderV` (`src/ri/render/rendererContext.cpp:1544-1564`) maps `"hidden"` (the RISpec name) to `reyes` silently, maps `"stochastic"` to `reyes` with the warning `Deprecated Hider "stochastic"; use "reyes" instead` (`CODE_BADTOKEN`), and stores any other string unvalidated in `COptions::hider` (default `reyes`, `options.cpp:263`); validation happens only at frame begin. `examples/rib/tests/bunny-reyes.rib` keeps `Hider "stochastic"` on purpose as the regression for the deprecated alias.
- **Capability-shaped flags written into core state by the hiders.** `CZbuffer::preDisplaySetup()` sets `HIDER_RGBAZ_ONLY` (`zbuffer.cpp:90`), `CPhotonHider::preDisplaySetup()` sets `HIDER_NODISPLAY | HIDER_ILLUMINATIONHOOK | HIDER_PHOTONMAP_OVERWRITE` (`photon.cpp:118`) into `CRenderer::hiderFlags`; the core then reads them (`rendererDisplay.cpp:627, 643` for "cannot handle display channels / arbitrary output variables", `:201` for no display, `rendererServicesImpl.h:61` for the illumination hook). They are static properties of a hider kind, stored as mutable core state. Two registry lines exist only because of the writes: `DB-011 link photon.cpp -> CRenderer::hiderFlags` and `DB-011 link zbuffer.cpp -> CRenderer::hiderFlags` (`allowlist.txt:127, 263`).
- **Other places the core knows concrete hider types or names.** The TSM output check `strcmp(hider, "reyes") != 0` (`rendererDisplay.cpp:770`, message `Hider must be reyes (or hidden, or the deprecated stochastic alias) for TSM`); `dynamic_cast<CReyes *>(contexts[0])` for `drawObject` (`renderer.cpp:1022`); the concrete hider headers: `renderer.cpp` includes all five (`photon.h`, `raytracer.h`, `reyes.h`, `stochastic.h`, `zbuffer.h`), `rendererContext.cpp` includes four (`photon.h`, `raytracer.h`, `stochastic.h`, `zbuffer.h`), and `rendererStatics.cpp` includes all five (`photon.h:51`, `raytracer.h:59`, `reyes.h:63`, `stochastic.h:69`, `zbuffer.h:74`) besides `hcshader.h` (corrected 2026-11-07); `geometryDispatch.cpp:66` includes `reyes.h` and is spec 031's; and `reyes.cpp:1993` reading `CRenderer::contexts` to reach its sibling threads (`DB-011 link reyes.cpp -> CRenderer::contexts`, `allowlist.txt:211`), which spec 029's plan (P-Q6 alternative c) handed to this spec.
- **`hcshader`.** `src/ri/hcshader.{h,cpp}` (90 + 708 = 798 lines) defines `CSphereLight` and `CQuadLight`, two `CShaderInstance`s constructed by `CRendererContext::getShader` only when no shader of that name was found (`rendererContext.cpp:345-360`, a `strcmp` on `"spherelight"`/`"quadlight"`). Their RSL replacements, `shaders/spherelight.sl` and `shaders/quadlight.sl`, ship, are compiled by the build and are exercised by `Visual_spherelight`, `Visual_spherelight-slo`, `Visual_quadlight` and `Visual_quadlight-slo` (`examples/rib/spherelight.rib`, `quadlight.rib`, `examples/rib/tests/*-slo.rib`). Spec 029 already removed their `friend` access to `CShadingContext`. `hcshader.cpp` reads `CRenderer::hiderFlags` (`:90, :404`) and is a Core file in the architecture domain map (`domain-map.txt:69`), a `ribRender` source (`src/ri/CMakeLists.txt:93`) and a file `libRIBVector` removes (`:394`); comments mention it at `:313, :326, :510`; `rendererStatics.cpp:43`, `renderer.cpp:38` and `rendererContext.cpp:49` include `hcshader.h`. A legacy page, `doc/Documentation/Hardcoded_shaders.html`, still documents the hard-coded shaders.
- **No `Integrator` anywhere.** There is no integrator concept, no `RiIntegrator`, no RIB token. Hider selection is the only axis: "transport" is whatever the `Surface` shader and the built-in `trace`/`gather`/`illuminance`/`indirectdiffuse` call sites do. `Hider` is an option-block statement (`check("RiHider", VALID_OPTION_BLOCKS)`, `ri.cpp:817`) stored in `COptions::hider`, with its own parameter list handled as `Option "hider"` (`rendererContext.cpp:1772`).
- **The constitution has nine domains.** Principle VIII's table (constitution v1.3.0) has no Transport domain; `tests/architecture/domain-rules.txt` mirrors an older version (its header says v1.2.0) and carries the allowed-dependency rows, `tests/architecture/domain-map.txt` maps directory prefixes to domains.
- **The plug-in convention exists.** Spec 037 gave every plug-in family `OR_ABI_VERSION`, an exported query symbol, a function table starting with `OrTableHeader`, and one routine, `checkPluginVersion`, with one `SPluginFamily` per family (`src/common/pluginAbi.h`). Decision D8: the built-in registries come first, behind these interfaces; run-time loading is spec 051.

### User Story 1 - Every existing scene renders as before (Priority: P1)

A scene author renders any existing RIB, with `Hider "raytrace"`, `"reyes"`, `"zbuffer"`, `"photon"`, the RISpec name `"hidden"`, the deprecated `"stochastic"` or no `Hider` statement at all, and gets the same bytes, the same diagnostics and the same exit behaviour as on the starting tree, while the choice of hider is made through a registry instead of two `strcmp` chains.

**Why this priority**: It is the acceptance of the whole spec (decision D2, "byte-identical"). Nothing else is worth merging if this fails.

**Independent Test**: Run the visual, parity and determinism suites on the finished tree and compare every output with the baseline recorded on the starting tree; run `bunny-reyes.rib` and a RIB with `Hider "bogus"`.

**Acceptance Scenarios**:

1. **Given** any scene of the visual, parity or determinism suites, **When** it renders on the finished tree, **Then** its bytes equal the starting tree's, on macOS and on Linux.
2. **Given** `Hider "stochastic"`, **When** the scene loads, **Then** exactly one deprecation warning with today's text is reported and the render equals `Hider "reyes"`; **Given** `Hider "hidden"`, **Then** no warning is reported and the render equals `Hider "reyes"`.
3. **Given** `Hider "bogus"`, **When** the frame begins, **Then** the error `Hider "bogus" unavailable` is reported and the frame renders with the reyes hider, as today.
4. **Given** `Hider "zbuffer"` and a display channel other than RGBAZ, **When** displays are set up, **Then** the same `cannot handle display channels` error is reported; **Given** `Hider "photon"`, **Then** no display is set up and the photon maps are written as today.

---

### User Story 2 - A contributor adds a hider without touching the core (Priority: P1)

A contributor writes a new hider in its own source file, registers it (name, aliases, declared capabilities, how it is constructed, how its jobs are dispatched), links it, and `Hider "<name>"` works. No file of the renderer core is edited: no `#include` of the new class, no new `else if` branch, no new alias line, no new flag.

**Why this priority**: It is the issue's own acceptance ("adding a hider needs no edit in the renderer core") and the point of DB-013.

**Independent Test**: A test translation unit registers a test-double hider under a new name and an alias; a RIB selects it and a frame is produced through it; the test lists the renderer-core files it did not need to change (none).

**Acceptance Scenarios**:

1. **Given** a hider registered from a file outside the renderer core, **When** a scene says `Hider "<its name>"` or its alias, **Then** that hider is constructed once per render thread and its jobs are dispatched by the way it declared.
2. **Given** two registrations with the same name or an alias that collides with a name, **When** the second registers, **Then** it is refused with a diagnostic naming both, and the first stays registered.
3. **Given** a plain static link of a program against the static renderer library (a new test, `Link_StaticConsumerRegistries`, built like `Link_StaticConsumer`), **When** it renders, **Then** every built-in hider and the `classic` integrator are found (a registrar the linker dropped is a failure of this spec).
4. **Given** the finished tree, **When** the core's sources are searched, **Then** none includes a concrete hider header, none names a concrete hider class, and none compares the hider's name with a string literal.

---

### User Story 3 - A scene author selects an integrator (Priority: P2)

A scene author writes `Integrator "classic"` (RIB) or calls `RiIntegrator("classic", ...)` (API, Python and Lua helpers included) and gets exactly the render they get without the statement. The statement exists so that later specs (042 `direct`, 050 `pathtracer`) add names, not grammar.

**Why this priority**: It makes the abstraction real and testable, but with `classic` the only implementation it changes no image.

**Independent Test**: Render a scene with `Integrator "classic"`, with no `Integrator`, and with `Integrator "bogus"`; compare images and diagnostics; round-trip the statement through `ribOut`.

**Acceptance Scenarios**:

1. **Given** a scene with `Integrator "classic"` and the same scene without it, **When** both render, **Then** the images are byte-identical.
2. **Given** `RiIntegrator`/`Integrator` in the RIB output (`ribOut`), **When** a scene containing it is parsed and written again, **Then** the statement is preserved.
3. **Given** `Integrator "bogus"`, **When** the frame begins, **Then** an error names the integrator and lists the registered names, the frame renders with `classic`, and `orender` exits -1 (FR-024).
4. **Given** the statement inside the world block (where option statements are invalid), **When** it is parsed, **Then** the same error as for `Hider` there is reported.
5. **Given** `Integrator "classic" "int maxdepth" [5]`, **When** the frame begins, **Then** the parameter is diagnosed by name, as an error (`RiLastError` set, so `orender` exits -1), as not defined by `classic`, and the frame still renders (FR-021).

---

### User Story 4 - A hider and an integrator that do not fit are refused, with a reason (Priority: P2)

A contributor (or a scene author) combines a hider with an integrator whose requirements the hider does not provide. Instead of producing a wrong or empty image, the frame is refused (not rendered, no fallback) with an error that names the hider, the integrator and the capability that is missing or too old. The pair is checked after the hider and the integrator have each been resolved with their own fallbacks.

**Why this priority**: It is the "no drop-offs" rule (D28) in its smallest testable form, and what 042 and 050 build their compatibility matrix on. With `classic` the only integrator, only a test double can exercise it.

**Independent Test**: A test registers a test-double integrator that requires a capability no built-in hider provides, and one that requires a newer version of a provided capability; pairs each with each hider; checks the diagnostics, and checks that `classic` pairs with all four built-in hiders.

**Acceptance Scenarios**:

1. **Given** `classic` and each of `reyes`, `zbuffer`, `raytrace` and `photon`, **When** the pair is checked, **Then** all four are accepted and render as today.
2. **Given** an integrator requiring capability C and a hider that does not declare C, **When** the pair is checked, **Then** the frame is refused and the error names the hider, the integrator and C.
3. **Given** an integrator requiring capability C at version V and a hider declaring C at an older version, **When** the pair is checked, **Then** the frame is refused and the error names both versions.
4. **Given** an integrator that lists capability O as optional and a hider that does not provide it, **When** the pair is checked, **Then** the pair is accepted and the integrator can see that O is absent.
5. **Given** an unknown hider name and an unknown integrator name, **When** the frame begins, **Then** both fall back (reyes, `classic`) and the pair check runs on the fallbacks, so the frame renders.

---

### User Story 5 - The hard-coded area lights are gone (Priority: P2)

A maintainer deletes `hcshader.{h,cpp}`, `CSphereLight`, `CQuadLight`, the `getShader` fallback and everything that referenced them, because the RSL `spherelight` and `quadlight` ship and are tested. A scene that used the shaders keeps rendering; a scene that names one without a compiled shader on the path now fails like any missing shader.

**Why this priority**: It is half of #36, removes 798 lines of a second implementation of the same lights, and unblocks 057, which touches the same `getShader` code.

**Independent Test**: The four spherelight/quadlight visual tests pass unchanged; a scene naming `spherelight` with an empty shader path reports `Failed to find shader "spherelight"`; a search finds no reference to the deleted classes.

**Acceptance Scenarios**:

1. **Given** the finished tree, **When** `Visual_spherelight`, `Visual_spherelight-slo`, `Visual_quadlight` and `Visual_quadlight-slo` run, **Then** they pass with their references and thresholds unchanged.
2. **Given** a scene `LightSource "spherelight"` and a shader path without a compiled `spherelight`, **When** the light is created, **Then** `Failed to find shader "spherelight"` is reported (`CODE_NOSHADER`), exactly as for any other missing shader.
3. **Given** the finished tree, **When** the sources, CMake files, architecture domain map and comments are searched, **Then** `hcshader`, `CSphereLight` and `CQuadLight` appear only in history records (specs, registry history, NEWS).

---

### User Story 6 - Principle VIII knows about integrators (Priority: P2)

A maintainer reading the constitution, the architecture check and the registry finds the same picture: a tenth domain, **Transport**, holds the integrators; it may depend on Graphics State, Geometry and Shading; Hiders may depend on it; the check enforces exactly that and nothing else changed. DB-013 is Resolved with this spec's number and date, the lines the work removes are gone from the allowlist, and nothing was added.

**Why this priority**: Decision D30 lands the amendment with the first spec that needs it, and the integrator registry needs a legal home.

**Independent Test**: `ctest --test-dir build -L architecture` passes with the new domain row; the allowlist diff shows deletions only; the constitution's table and the check's rule file agree.

**Acceptance Scenarios**:

1. **Given** the finished tree, **When** the architecture check runs on macOS and Linux, **Then** it passes, a file under the Transport directory is classified as Transport, and an include from Transport to Hiders (a cycle) fails a fixture.
2. **Given** the registry, **When** DB-013 is read, **Then** it is under Resolved with "spec 030" and the date, its evidence line numbers are current, and DB-011's history records the lines removed.

---

### Edge Cases

- **Unknown hider name**: today's behaviour is kept (error at frame begin, then the reyes hider; Acceptance Scenario 1.3).
- **`Hider` given twice**: the last wins, as today (`RiHiderV` frees the previous string).
- **An alias that points at a name registered later or never**: an alias resolves at lookup time; an alias whose target is missing is a registration error (diagnostic naming both), not a silent drop.
- **Registrar dropped by the linker** (gotcha 31): detected by a test that links statically and lists the registered names.
- **Hider name case and surrounding whitespace**: unchanged from today (exact, case-sensitive match).
- **`Hider "photon"` with `Integrator "classic"`**: accepted; the photon pass keeps using the illumination hook (the capability is declared by the hider, FR-009).
- **`Integrator` before and after `Hider`**: order does not matter; both are read at frame begin.
- **Several frames in one process** (`RiFrameBegin`/`RiFrameEnd`, repeated `RiBegin`): the registries are process-wide and immutable after start-up; per-frame state is rebuilt each frame.
- **Network rendering** (`netClient`): servers construct hiders through the same registry; the existing single-thread rule for servers is unchanged.
- **A TSM (deep shadow) output with a non-reyes hider**: still an error; the check reads a declared capability instead of comparing a name, and keeps the diagnostic's meaning.
- **Scene names a light shader that is not compiled** (US5): now `Failed to find shader`; before, `spherelight` and `quadlight` silently used the C++ version. Behaviour change accepted by the maintainer (clarify session 2026-11-06), recorded in NEWS. With the shipped shaders on the path the compiled `.sl` is found first, so no image changes.
- **Unknown hider name with a TSM output**: `CRenderer::hider` keeps the bad name, so the TSM check still errors for it, as today; the registry's fallback to reyes must not make it pass.
- **Unknown hider and unknown integrator together**: each falls back, then the pair is checked on the fallbacks (US4 AS5).
- **Abnormal inputs to the new statement**: `Integrator` with no name, a non-string name, or parameters `classic` does not define are diagnosed (FR-021); none crashes.

## Requirements *(mandatory)*

### Functional Requirements

**Hider registry**

- **FR-001**: The renderer MUST select, construct and set up its hider through a registry of hider kinds. A registration carries: the canonical name, zero or more aliases (each optionally marked deprecated, with its warning text), the declared capabilities with versions (FR-009), and the means to construct the hider for a render thread and to run its pre-display setup and job dispatch.
- **FR-002**: Both `strcmp` chains of `CRenderer::beginFrame()` and the `"hidden"`/`"stochastic"` mapping of `RiHiderV` MUST be replaced by registry lookups. `raytrace`, `reyes`, `zbuffer` and `photon` are registered by their own source files; `hidden` is a non-deprecated alias of `reyes`; `stochastic` is a deprecated alias of `reyes` with today's warning text.
- **FR-003**: Adding a hider or an alias MUST NOT require editing any renderer-core file in the matter of selection, construction, pre-display setup, job dispatch or capability flags. A test registers a test-double hider from a file outside the core and renders through it (US2 AS1).
- **FR-004**: A duplicate name, an alias colliding with a name or another alias, and an alias whose target does not exist MUST each be refused at registration with a diagnostic naming the entries involved; the first registration stays.
- **FR-005**: The unknown-name path MUST keep today's behaviour: the error `Hider "<name>" unavailable` with `CODE_BADTOKEN` at frame begin, then a frame rendered with the reyes hider (including the job-dispatch choice). `CRenderer::hider` keeps the unknown name, so messages that print it and the TSM check (FR-007) behave as today.
- **FR-006**: Registration MUST survive a plain static link: `ribRender_static` consumers find every built-in hider and `classic` with no force-load, whole-archive or export-dynamic option (spec 029 removed them; gotcha 31's anchor technique applies), proved by a new test, `Link_StaticConsumerRegistries`, built like `Link_StaticConsumer`, that lists the registered names and renders with each built-in hider; the approved `Link_StaticConsumer` stays unchanged.
- **FR-007**: The renderer-core sources involved in hider selection (`renderer.cpp`, `rendererContext.cpp`, `rendererStatics.cpp`, `rendererDisplay.cpp`, `rendererJobs.cpp`) MUST NOT include a concrete hider header, name a concrete hider class, or compare the hider name with a literal. This covers the TSM check (`rendererDisplay.cpp:770`, now a capability) and the `dynamic_cast<CReyes *>` of `renderer.cpp:1022` (replaced by the `rasterization` capability and a virtual `drawObject` on the `CHider` base, which the existing `CReyes::drawObject`, `reyes.h:247`, overrides; no new Reyes method). The TSM check keeps its result for an unknown hider name (it still errors). It does NOT cover `geometryDispatch.cpp`'s `dice(CReyes *)` family, which is spec 031's.
- **FR-008**: The hider's declared static properties MUST replace the writes into `CRenderer::hiderFlags` that exist only to describe a hider kind (`HIDER_RGBAZ_ONLY`, `HIDER_NODISPLAY`, `HIDER_ILLUMINATIONHOOK`, `HIDER_PHOTONMAP_OVERWRITE`). The core reads them from the registration. The run-time bits that are frame state (`HIDER_BREAK`, `HIDER_DONE`) stay where they are. The two allowlist lines `photon.cpp -> CRenderer::hiderFlags` and `zbuffer.cpp -> CRenderer::hiderFlags` MUST be deleted, and the line `reyes.cpp -> CRenderer::contexts` (`allowlist.txt:211`) MUST be deleted by giving the hider a way to reach its sibling threads that does not read the core, for example a per-frame peer group the registry's construction hands to each per-thread hider (spec 029, P-Q6 alternative c; maintainer ruling 2026-11-06). The line `reyes.cpp -> CRenderer::numThreads` (`allowlist.txt:176`) MAY be deleted by the same mechanism if the peer group also carries the thread count; it is optional. `HIDER_GEOMETRYHOOK` (defined, never used) is not carried over. The four `tests/shading_parity` tests that switch `HIDER_ILLUMINATIONHOOK` through `CRenderer::hiderFlags` are edited, after a diff-first pause, to switch the frame trait instead (amendment F1, 2026-11-09; Source reconciliation R20).

**Capabilities and integrators**

- **FR-009**: A capability is a property identified by a string name plus a version (not by a bit position, so later specs add names without renumbering and the set carries versions). One vocabulary covers both what integrators need and the hiders' static properties the core reads; the absence of a capability means the hider is limited in that respect (for example `zbuffer` lacks arbitrary display channels, `photon` lacks display output, only `reyes` provides deep-shadow output). Hiders declare the capabilities they **provide**; integrators declare the ones they **require** and the ones they **optionally use**, each at a minimum version. The vocabulary of this spec is limited to what the four built-in hiders and `classic` actually declare or need (the plan fixes five: `rasterization`, `display-output`, `display-channels`, `deep-shadow-output`, `illumination-hook`; ray intersection, ray occlusion and the others are reserved names); the other names of the reference document's §5.3 are reserved and are added by the spec that needs them, as MINOR steps of the capability interface.
- **FR-010**: An integrator registry MUST exist, built the same way as the hider registry (name, aliases, declared requirements, construction), with `classic` registered by its own source file and as the default when no `Integrator` is given. The core calls the integrator's `configure`, `prepare` and `finish` at frame begin and end (FR-016). Registration is built-in now; run-time loading is spec 051 and is not implemented here.
- **FR-011**: The hider and integrator interfaces MUST follow the shared plug-in convention (spec 037) at **registration level**. The hider table carries name, aliases, capabilities, create/destroy returning an opaque handle, pre-display setup and dispatch kind, plus an optional integrator field (FR-014); the hider's runtime (`CHider`, `renderingLoop`, the tracer and producer wiring) stays internal C++, documented as not external until spec 051. The integrator table carries name, aliases, requirements and the `configure`/`prepare`/`finish` slots (FR-016). Each is a C function table starting with `OrTableHeader`, a semantic version `OR_ABI_VERSION(major, minor, patch)`, opaque handles, defined ownership and error codes, a C++ convenience header, and a version-history table in the family header. Built-in registrations go through the same tables, and the versions are checked by `checkPluginVersion` through one new `SPluginFamily` per interface (a family adds an entry, never a second check). The starting version of each interface follows the project policy "MAJOR stays 2 until spec 055" (the plan chooses the number and records the rule).
- **FR-012**: Before a frame renders, the pair (hider, integrator) MUST be checked: every required capability must be provided at a version at least the required one. The check runs on the resolved pair, after the hider and the integrator have each been resolved with their own fallbacks (FR-005, FR-024). A failing pair MUST be refused, meaning the frame is not rendered and there is no fallback, with an error naming the hider, the integrator, and each missing capability or version shortfall. A missing optional capability is not a failure and is visible to the integrator.
- **FR-013**: `classic` MUST declare no required capability beyond what the four built-in hiders provide today, so `classic` pairs with `reyes`, `zbuffer`, `raytrace` and `photon` (D28's cells for `Surface` shading under `classic`). The hider x integrator compatibility of every future cell is recorded where 042 can extend it (a test table), not enforced by a hard-coded list in the core.
- **FR-014**: The registries MUST be built so that an alias can later stand for a (hider, integrator) pair (`Hider "pathtrace"` = `raytrace` + `pathtracer`, decisions D4/spec 050). This spec implements aliases to one hider only; the registration shape carries an optional integrator field so the pair form is not a breaking change. Precedence of an alias's integrator against an explicit `Integrator` statement is deferred to spec 050.
- **FR-015**: Registered entries MUST be enumerable (names, aliases, capabilities, versions) for diagnostics and tests, and the unknown-name diagnostic of an integrator lists the registered names.
- **FR-016**: `classic` MUST be a registered integrator with its declared requirements (none beyond FR-013) and a pass-through lifecycle: the core MUST call its `configure`, `prepare` and `finish` at frame begin and end, so the lifecycle is exercised from this spec on. Today's transport call sites (the shader's own `illuminance`, `trace`, `gather`, `indirectdiffuse`, the photon pass's hook) are untouched and stay in Shading (Transport may depend on Shading, never the reverse), so byte-identity is safe by construction. The three lifecycle slots are pinned in the interface's version-history table; spec 042 adds the render-work and services parts (reference doc §5.2, §5.4, §5.5) as MINOR steps that extend, never reshape, those slots.

**RI / RIB surface**

- **FR-017**: `RiIntegrator`/`RiIntegratorV` MUST be added to the RI API (`ri.h`, the `CRiInterface` default no-op, the renderer context, `ribOut`, the Python and Lua `prman` helpers), and an `Integrator` statement to the RIB grammar and lexer, through all four layers a new token needs: token constants (the three lists of gotcha 22: `ri.h`, `riVocabulary.h`, `riTokens.cpp`, checked by `Common_VocabularyConsistency`), RIB parsing and option storage (`COptions`, a new field next to `hider`), and pre-declaration as FR-020 states it (none: integrator parameters use inline declarations). No `COptions::find()` entry or `RiGetOption`-style query is added, because `Hider` has none for its name (`COptions::find` answers only the hider parameters; corrected 2026-11-07). The addition is backward compatible (a MINOR step of the RI header's documentation, if it has one). The Python and Lua `prman` helpers only write RIB text (`prman.py:280`, `prman.lua:331` for `Hider`), so they gain an `Integrator` writer the same way.
- **FR-018**: The statement is valid wherever `Hider` is valid (option blocks), is rejected elsewhere with the same error `Hider` gets, and the last statement before the frame wins. A scene without `Integrator` behaves exactly as `Integrator "classic"`.
- **FR-019**: `ribOut` MUST write `Integrator` back so that parse then write preserves it, and the preview back-end (`CPreviewContext`, which inherits the default) MUST ignore it without a diagnostic.
- **FR-020**: The statement's form is `Integrator "name" [parameters...]`, the `Hider` form, stored in `COptions` beside `hider`; the handle of the RIS form is not part of the grammar. The parameter name `handle` (a string) is reserved so a handle can be added later without a grammar change. Parameters are stored as an opaque typed name/value list in Graphics State, which knows no integrator (it cannot depend on Transport): names the Scene Interface does not pre-declare use inline declarations (`"int maxdepth" [5]`), the parser's existing rule, and no core pre-declaration list is added for integrator parameters. The integrator validates the list at frame begin (FR-021).
- **FR-021**: Parameters given to an integrator that does not define them (all of them, for `classic`) MUST be diagnosed per parameter at frame begin, as an **error** (`RiLastError` is set, so `orender` exits -1, like an unknown `Option`; maintainer answer Q4), with the integrator's name and the parameter's, and MUST NOT stop the frame.
- **FR-022**: The integrator choice MUST be an immutable per-frame value for the render threads (read-only once the frame starts), consistent with how spec 028 installs `FrameParams`.

**Failure policy**

- **FR-023**: Every diagnostic this spec adds uses the project's existing diagnostic codes and the one-per-event rule (a deprecation warning is reported once per statement, not once per thread).
- **FR-024**: Failure policy is split. An **unknown integrator name** MUST report an error that lists the registered names, then fall back to `classic` (as an unknown hider falls back to reyes, FR-005); the error sets `RiLastError`, so `orender` exits -1 as for any error. An **unsupported pair** MUST be refused: the frame is not rendered and there is no fallback (decision D28, "fails with an explicit diagnostic", so the compatibility matrix of 042 can tell a failing cell from a rendering one). The order is: resolve the hider with its fallback, resolve the integrator with its fallback, check the pair on the resolved ones. The refusal MUST NOT terminate the process (the default sink's severe path exits) and MUST NOT crash in a world block; how a refused frame passes through `RiWorldEnd` and what state it leaves is for the plan, which MUST stop and ask the maintainer if it proves too invasive. With `classic` the only integrator, only a test double reaches the refusal today.

**hcshader retirement**

- **FR-025**: `src/ri/hcshader.{h,cpp}`, `CSphereLight`, `CQuadLight`, the `getShader` fallback (`rendererContext.cpp:345-360`) and every reference MUST be deleted: the `ribRender` and `libRIBVector` source lists, the stale comments that explain them (`src/ri/CMakeLists.txt:313, 326, 510`), the `domain-map.txt` line, the `hcshader.h` includes of `renderer.cpp`, `rendererStatics.cpp` and `rendererContext.cpp`, and the remaining `hiderFlags` reads. If removing the files lets `rendererStatics.cpp` or another file leave `tests/architecture/ribvector-core-residual.txt`, the list is shortened (it only shrinks).
- **FR-026**: After the deletion, a light shader named but not found MUST produce the generic missing-shader error. The change in behaviour for `spherelight` and `quadlight` (previously a silent C++ substitute, reachable only when the compiled shader is not on the shader path) is accepted by the maintainer and recorded in NEWS and in the validation record.
- **FR-027**: `Visual_spherelight`, `Visual_spherelight-slo`, `Visual_quadlight` and `Visual_quadlight-slo`, the photon-pass and sampling tests that use these lights (`Sampling_ThreadSweep_quadlight`), and every other test that renders a scene with them MUST pass with their references and thresholds unchanged. A test or scene that relied on the C++ substitute is repointed at the compiled shader, never loosened.
- **FR-028**: The legacy page `doc/Documentation/Hardcoded_shaders.html` MUST be corrected or removed so that no maintained document describes the retired lights as hard-coded renderer features; `quadlight.sl` and `spherelight.sl` themselves are untouched (their deprecation belongs to spec 057).

**Governance and records**

- **FR-029**: The constitution MUST be amended (Principle VIII) to add the tenth domain **Transport** (owns integrators; may depend on Graphics State, Geometry and Shading; Hiders may depend on it; no dependency of Transport on Hiders, the Scene Interface or the renderer core), with a Sync Impact Report and a MINOR version step. The amendment is made by the implement stage through the `speckit-constitution` workflow with the maintainer's approval; this specify stage does not edit it.
- **FR-030**: The architecture check MUST know the domain: `tests/architecture/domain-rules.txt` gains the Transport row and Hiders' new allowed edge (and its header is brought to the constitution's version), `domain-map.txt` maps the Transport directory, and a fixture proves both the allowed edge and a cycle (Transport to Hiders) are classified as the table says. Allowlist lines are only deleted; none is added or re-keyed.
- **FR-031**: `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` MUST be updated: DB-013 moves to Resolved with "spec 030" and the date and the measured end state; DB-011's size and history record the lines removed; DB-020 gets a history note only if the work changes it (spec 029 already removed the light friends, and the remaining friends belong to spec 032); stale line references in DB-013 are corrected in the same edit.
- **FR-032**: `tests/architecture/metrics-baseline.txt` MUST be lowered to the measured end values if any reference goes, and no class's count may rise.
- **FR-033**: No task, requirement or text of this spec MAY depend on the merge. Registry entries move to Resolved with this spec's number and date; issue #36 closes through a `Closes #36` line the maintainer writes in the merge commit, which this spec does not write.
- **FR-034**: `CLAUDE.md` and `DEVNOTES.md` MUST be updated at the end of implementation, not before: the Hider system paragraph ("Adding a hider = new `#include` + `if/else if` branch" becomes the registration recipe), the architecture section (ten domains, Transport), the Active Technologies and Recent Changes entries, a new known gotcha for the registries, and the Spec-kit and spec index entries. This specify stage edits neither.

**Verification**

- **FR-035**: Every image of the visual, parity and determinism suites MUST be byte-identical to the baseline recorded on the base commit, and every `MaxBlockAvgDiff` identical, on macOS and, against its own baseline, on Linux. No threshold is widened and no test is disabled. The full run excludes nothing the starting tree runs.
- **FR-036**: New tests MUST cover: registry lookup, aliases (deprecated and not), duplicate and collision refusal, the unknown-name fallback, the test-double hider and integrator registered from outside the core, capability negotiation (accept, missing, version shortfall, optional), `Integrator` parsing, option storage, `ribOut` round-trip and the Python/Lua helpers, static-link registration, and the architecture fixtures of FR-030.

### Key Entities

- **Hider kind**: a registered visibility strategy: name, aliases, declared capabilities, how to construct it per render thread, how to set up displays before the frame, how its jobs are dispatched.
- **Hider registry**: the process-wide, start-up-built table of hider kinds; the only place a hider's name is resolved.
- **Integrator kind** and **integrator registry**: the same for transport strategies; `classic` is the only built-in.
- **Capability**: a named, versioned property that hiders provide and integrators require or optionally use.
- **Alias**: a second name for a registered kind, optionally deprecated with a warning text; later able to stand for a (hider, integrator) pair.
- **Pair check**: the pre-frame negotiation between one hider kind and one integrator kind.
- **`Integrator` statement / `RiIntegrator`**: the Scene Interface's way to choose the integrator; stored in the options like `Hider`.
- **Transport domain**: the tenth Principle VIII domain; owns integrators and the capability vocabulary Hiders reference.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every scene of the visual, parity and determinism suites produces output byte-identical to the baseline recorded on `e83d005d`, on macOS and on Linux, with every `MaxBlockAvgDiff` unchanged and no threshold changed (US1, FR-035).
- **SC-002**: A test-double hider and a test-double integrator, each registered from a file outside the renderer core, are selected from RIB and used for a frame; the list of renderer-core files that had to change to support them is empty (US2, FR-003).
- **SC-003**: A search of the renderer-core hider-selection files finds no concrete hider header include, no concrete hider class name and no `strcmp` against a hider name (FR-007); `CRenderer::beginFrame()` contains no hider-specific branch.
- **SC-004**: `Hider "stochastic"` reports one deprecation warning and equals `Hider "reyes"`; `Hider "hidden"` reports none; `Hider "bogus"` reports the unavailable error and falls back to reyes; `bunny-reyes` still passes (US1).
- **SC-005**: `Integrator "classic"` and an absent statement produce identical images for the four built-in hiders; the statement round-trips through `ribOut`; `RiIntegrator` exists in the C API and the Python and Lua helpers (US3, FR-017 to FR-019).
- **SC-006**: Capability negotiation accepts `classic` with all four hiders and refuses the frame (no image, no process exit, no crash) for each of the test-double failures (missing capability, version shortfall) with an error that names the hider, the integrator and the capability; an optional shortfall is accepted; an unknown integrator name falls back to `classic` with an error and `orender` exit -1; `classic`'s `configure`/`prepare`/`finish` are observed to be called once per frame (US3, US4, FR-016, FR-024).
- **SC-007**: `hcshader.{h,cpp}` are deleted, no file outside history records names `hcshader`, `CSphereLight` or `CQuadLight`, the four spherelight/quadlight visual tests pass with unchanged references, and a missing `spherelight` shader gives `Failed to find shader` (US5).
- **SC-008**: A program linked plainly against the static renderer library finds every built-in hider and `classic` (FR-006).
- **SC-009**: The architecture check passes on macOS and Linux with the Transport row; the allowlist diff is deletions only (at least the three lines named in FR-008 if achievable, plus any the plan identifies); the constitution's domain table and the check's rule file name the same dependencies; `DOMAIN_BOUNDARIES.md` shows DB-013 under Resolved with "spec 030" and the date (US6, FR-029 to FR-032).
- **SC-010**: A representative reyes scene and a representative raytrace scene render within the run-to-run noise of the starting tree on the same machine and thread count: the registry costs only start-up and frame-begin lookups, not per-sample work.
- **SC-011**: No task and no text of the spec set refers to the merge, a merge-commit hash or a post-merge step.

## Source reconciliation (plan vs issue vs code)

The coordinator asked for every difference found; this section is kept so a reviewer can overrule any entry cheaply.

| # | Source says | The code or another source says | Treatment |
|---|---|---|---|
| R1 | Issue #36 scope: hider factory and `hcshader` removal only | The plan adds the integrator registry, `RiIntegrator`/`Integrator` with `classic`, capability negotiation, the D30 Transport amendment | The plan's scope is authoritative (Clarifications) |
| R2 | Issue and plan: resolves DB-020 (the light friends) | Spec 029 already removed `CShadingContext`'s friends `CPhotonHider`, `CSphereLight`, `CQuadLight`; DB-020's remaining friends (remote channels, `CBrickMapGeometry`) are spec 032's | This spec does not claim DB-020; FR-031 allows a history note only |
| R3 | Plan: hcshader 686 lines; DB-013: 776 lines | 798 lines (708 + 90) | Measured value used |
| R4 | Plan: `renderer.cpp:747-793`, `rendererContext.cpp:325-338,1406-1413`, `rendererDisplay.cpp:768` | Chains at `renderer.cpp:778-792, 815-834`; hcshader fallback `rendererContext.cpp:345-360`; `RiHiderV` `:1544-1564`; TSM check `rendererDisplay.cpp:770` | Current numbers used |
| R5 | DB-013: alias at `rendererContext.cpp:1529`, chains at `renderer.cpp:778-830` | Alias at `:1554`; chains as above | FR-031 corrects the registry text |
| R6 | `domain-rules.txt` header: mirrors constitution v1.2.0 | The constitution is v1.3.0 | FR-030 brings the header to the amended version |
| R7 | Plan: V17 (lights) is 030's | Done by 029 (friends removed) | Nothing to do for the friends; the files go with FR-025 |
| R8 | CLAUDE.md: "Hider selection ... (~line 815)" and "Adding a hider = new `#include` + `if/else if` branch" | True today (line 815 is the second chain) and false after this spec | FR-034 updates it at the end |
| R9 | Plan: "all four attribute/option layers" for the Integrator statement | `Hider` is an option, not an attribute: layers are token lists (three files, gotcha 22), parse, `COptions` storage, pre-declaration | FR-017 spells them out |
| R10 | Plan 057: "030 retires the `hcshader` fallback in `getShader`, the code the AreaLightSource binding touches" | Confirmed: the fallback is `rendererContext.cpp:345-360` inside `getShader` | FR-025; 057 starts from a `getShader` without the fallback |
| R11 | Spec 039 FR-022: hcshader lights draw from transport sub-streams until 030 replaces them | Moot once the files are deleted | No action; spec 039's text stays as history |
| R12 | Spec 029 plan P-Q6 (c): reyes reading `CRenderer::contexts` is 030's | Still one allowlist line (`reyes.cpp -> CRenderer::contexts`); the same loop also reads `CRenderer::numThreads` (`allowlist.txt:176`) | FR-008 (MUST for `contexts`, optional for `numThreads`) |
| R13 | Specify stage: `rendererStatics.cpp` includes concrete hider headers | Confirmed by the plan (2026-11-07): it includes all five plus `hcshader.h`; `rendererContext.cpp` includes four, `renderer.cpp` five. The specify-stage correction to "none" was wrong and is reverted | Starting point corrected; FR-007 covers the file |
| R14 | `HIDER_GEOMETRYHOOK` (`renderer.h:52`) looks like a hider property | Defined, never used | Not carried into the capabilities (FR-008) |
| R15 | Marker FR-024 offered "fall back to `classic`" for a pair; US4, FR-012 and SC-006 said "refused" | Internally inconsistent | Reconciled: unknown name falls back, unsupported pair is refused (FR-024) |
| R16 | Starting point (before 2026-11-07): `rendererStatics.cpp` includes no hider header | `photon.h:51`, `raytracer.h:59`, `reyes.h:63`, `stochastic.h:69`, `zbuffer.h:74`, `hcshader.h:43` | Corrected (starting point, FR-025) |
| R17 | FR-008 lists `HIDER_PHOTONMAP_OVERWRITE` among the flags that describe a hider kind | Set at `photon.cpp:118`, defined at `renderer.h:51`, read nowhere | Deleted with no replacement capability (maintainer answer Q3) |
| R18 | FR-007: "an object-submission capability or interface on the hider base" | `CReyes::drawObject` already exists (`reyes.h:247`) | FR-007 names the `CHider` virtual it overrides |
| R19 | FR-017: "with `find()`" and "`RiGetOption`-style query where `Hider` has one" | `Hider` has no query for its name (`options.cpp:838` answers only the hider parameters) | FR-017 adds none |
| R20 | Plan "Existing tests" (and tasks T017 (a) as written): no test constructs or names `hiderFlags`, `HIDER_*` or a concrete hider | Four approved `tests/shading_parity` tests switch `HIDER_ILLUMINATIONHOOK` through `CRenderer::hiderFlags` (measured at Phase 1, 2026-11-09) | FR-008 stands; a pinned-test edit with a diff-first pause (tasks.md T109a) moves only those lines to the frame traits; assertions unchanged |

## Scope Boundaries

**In scope**: the hider registry and its self-registration; the integrator registry, `classic` and `Integrator`/`RiIntegrator` through all layers; capability declaration and negotiation; the plug-in-convention shape of both interfaces; hcshader retirement; the Transport domain amendment and the architecture check's row; registry, metrics and records.

**Out of scope, and whose it is**:

| Item | Placement | Reason |
|---|---|---|
| `Integrator "direct"`, the compatibility-matrix suite of `Bxdf` cells | spec 042 | 030 ships `classic` only |
| `pathtracer`, `Hider "pathtrace"` | spec 050 | FR-014 only keeps the door open |
| Run-time loading of hider/integrator modules, `LightFilter`/`DisplayFilter`/`SampleFilter` | spec 051 | D8 |
| The integrator's work units, services facade, shading points (reference §5.2, §5.4, §5.5) | specs 035, 041, 042 | unless FR-016 option B is chosen |
| `geometryDispatch.cpp`'s `dice(CReyes *)` family and the Dispatch/Stub pairs | spec 031 | DB-017 |
| GI cache split, the remaining DB-020 friends | spec 032 | |
| Volume shading, `Interior`/`Exterior`/`Atmosphere` | spec 056 | runs in parallel; takes none of this scope |
| `AreaLightSource` binding, deprecating `quadlight.sl`/`spherelight.sl`, light-surface globals | spec 057 | 030 only deletes the C++ fallback |
| Hider parameters (`Option "hider"`: jitter, emit, falsecolor, ...) moving out of Graphics State | later | the parameter list stays as is |
| Windows-specific code paths | untouched | no official Windows support, none proactively removed |

## Assumptions

- Spec 029 is merged (master `e83d005d`); 039, 026, 027, 028, 037 and 038 are merged. The base is master, no rebase is assumed.
- The byte-identity baseline is the post-039 output of the base commit (decision D2 as restated in spec 039).
- Hiders may depend on Shading, Geometry, Graphics State and Display Outputs today; the new edge Hiders to Transport is added by the amendment. The Scene Interface does not depend on Transport: it stores the integrator's name and parameters, and the renderer core validates them at frame begin (as it does for the hider).
- The Transport domain's directory and its domain-map prefix are chosen by the plan; the registries of hiders live in Hiders and the registry of integrators in Transport.
- The capability vocabulary is minimal, string-plus-version, and shared by integrator needs and hider static properties (FR-009); reserved names are added when a spec needs them.
- The hider's runtime stays internal C++ until spec 051; only its registration is behind the versioned table (FR-011).
- Integrator parameters need inline declarations; Graphics State holds them as an opaque list (FR-020).
- The starting version of the new interfaces follows the "MAJOR stays 2 until spec 055" policy; the plan fixes the numbers.
- Python and Lua `prman` helpers gain `Integrator` the way they have `Hider`.
- The unknown-hider diagnostic stays at frame begin (not moved to the statement) so its timing and text do not change.
- Builds and test runs go through `~/.claude/bin/cpujob` at `-j4` at most (`-j2` for sweeps); Linux is validated over ssh in a per-spec tree as in specs 026 and 029.
- No Windows support is assumed or removed.

## Open Questions

None. The three markers (FR-016, FR-020, FR-024) were resolved in the clarify session of 2026-11-06. Left to the plan by design: how a refused frame passes through `RiWorldEnd` (FR-024), and the precedence of an alias's integrator against an explicit `Integrator` (spec 050).
