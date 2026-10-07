# Domain Boundaries — Violation Registry

This file is the registry that constitution **Principle VIII (Domain Separation and Modular
Boundaries)** requires (rule 8). The domains, the allowed-dependency table and the rules themselves
live in `.specify/memory/constitution.md` (v1.2.0+); this file only tracks where the codebase does
not yet comply.

**How to use it**

- Every known violation gets an entry here, linked to a tracking issue.
- A change MUST NOT extend a registered violation; a change that touches the affected area SHOULD
  reduce it, and records the reduction in the entry's history.
- New violations are prohibited. A violation found (not introduced) during other work is added here
  with its evidence; the finding itself does not block that work.
- When an entry is fully resolved, move it to **Resolved** with the commit and date — don't delete it.
- Only record what the code demonstrably shows (a symbol list, a header, a link line), not suspicion.

## Open

### DB-001 — Shading resolves renderer-core symbols at load time

- **Rules**: 2 (acyclic dependencies; no upward dependency via load-time-bound symbols), and the
  renderer-core rule (no domain may depend on the renderer core).
- **Tracking**: [#24](https://github.com/juvenal/openrender/issues/24) (enhancement / design review)
- **Evidence**: `libshader_shading` (Shading) links with `-undefined dynamic_lookup` (macOS) /
  `--allow-shlib-undefined` (Linux) in `src/libshader/shading/CMakeLists.txt`, and binds 28
  distinct `libRIBRender` symbols flat-namespace (58 fixups per `dyld_info -fixups`):
  - renderer core: `CRenderer::globalMemory`, `getDSO`, `getFilter`, `getStepFilter`,
    `getGlobalID`, `retrieveVariable`, `parseVariable`;
  - Hiders: `CShadingContext::trace`/`traceEx`, `CGatherBundle`, `CTraceBundle`/
    `CTransmissionBundle` vtables, `CPhotonMap::lookup`;
  - Texturing: `CTexture3d::resolve`;
  - infrastructure: `CRefCounter`, `CFileResource`, `memoryInit`/`memoryTini`/`memoryNewPage`,
    `stats`, `error`/`info`/`warning`, `sampleHemisphere`/`sampleCosineHemisphere`.
- **Measured by the architecture check** (spec 022, macOS): 30 edges allowlisted under DB-001.
  - 26 link references from Shading sources: `CRenderer::getDSO`, `getFilter`, `getStepFilter`,
    `getGlobalID`, `retrieveVariable` and `globalMemory`; `CShadingContext::trace`/`traceEx`,
    `CGatherBundle`, the `CTraceBundle`/`CTransmissionBundle` vtables and `CPhotonMap::lookup`;
    and `error`/`warning`/`info`/`parseVariable`, which are defined in Scene Interface.
  - 4 includes of `src/ri/render/renderer.h` (`shader.cpp`, `shaderPl.cpp`, `executeMisc.cpp`,
    the generated `rslo` parser).
  - Not edges: `CTexture3d::resolve` is Shading → Texturing, which the table allows (a
    load-time binding problem only). `CRefCounter`, `CFileResource`, the memory arena and
    `stats` are defined in `src/ri/core`, i.e. infrastructure.
  - Shading's source-level coupling is registered separately: hider headers under DB-005,
    Geometry headers under DB-006, diagnostics and vocabulary headers under DB-016, and
    `dso.h` under DB-019.
- **Consequences today**: `libshader_shading` cannot load without `libRIBRender` in the process; a
  static engine link must whole-archive `libRIBRender.a` (the `openRender::RIBRender_static`
  package target applies `-force_load` for this — remove that workaround once this is resolved).
- **Direction**: move generic infrastructure (`CRefCounter`, `CFileResource`, memory arena, stats,
  diagnostics, sampling) down into a lower shared layer; invert the remaining Hiders/Texturing/
  renderer-core calls through interfaces Shading owns (trace/gather, photon/texture3d lookup,
  filter/variable/DSO services), implemented and registered by the higher layers. Options are
  analysed in #24.

### DB-002 — No automated domain-dependency check in CI

- **Rule**: 7 (CI MUST fail on dependency cycles and on cross-domain dependencies outside the
  allowed-dependency table).
- **Tracking**: [#25](https://github.com/juvenal/openrender/issues/25) (enhancement)
- **Evidence**: `.github/workflows/` holds only `deploy-site.yml` and `release.yml`; neither runs a
  dependency-graph check, and no such test exists under `tests/`.
- **Direction**: a check over the CMake target graph (e.g. `cmake --graphviz`) and/or the link-level
  symbol graph (`nm`/`dyld_info`, which is what exposes DB-001), mapping targets/paths to domains
  and failing on any edge the table forbids. Until the source tree is split into one target per
  domain, the check has to map by source directory; it should start by encoding DB-001 as a known,
  allowed-for-now exception rather than failing on day one.

### DB-003 — Plug-in ABI headers carry no interface version

- **Rule**: 4 (plug-in interfaces MUST carry a Semantic Versioning 2.0.0 interface version; the host
  MUST reject a MAJOR mismatch).
- **Tracking**: [#26](https://github.com/juvenal/openrender/issues/26) (enhancement)
- **Evidence**: none of the published plug-in headers declares a version — `src/ri/render/shadeop.h`
  (Shader DSO Operands), `src/ri/geometry/dlo.h` and `src/ri/geometry/implicit.h` (Geometric
  Procedures), `src/display/dsply.h` (Display Outputs) — and the loaders therefore cannot check one.
- **Direction**: add a `MAJOR.MINOR.PATCH` interface version per header (macros plus an exported
  query symbol each plug-in provides), have the host validate it at load time, and document each
  header's version history. Existing plug-ins without the symbol need a defined transition rule
  (e.g. treated as `1.0.0` with a warning for one release).

### DB-004 — Geometry depends on the renderer core and on Scene Interface

- **Rule**: the renderer-core rule ("no domain MAY depend on the renderer core"); the
  allowed-dependency table (Geometry may depend only on Graphics State and Geometric Procedures, not
  on Scene Interface); and 2 (an upward capability must be inverted through an interface the lower
  domain owns).
- **Tracking**: [#27](https://github.com/juvenal/openrender/issues/27)
- **Evidence**:
  - 19 files under `src/ri/geometry/` include `renderer.h`/`rendererContext.h` and use 24 distinct
    renderer-core symbols (`grep` over `src/ri/geometry/*.{h,cpp}`, master `079feda`).
  - Frame and camera parameters:
    - `CRenderer::invShutterTime` (24 uses), `numThreads` (9), `maxGridSize` (9), `relRotQ` (4),
      `maxEyeSplits` (3), `flags` (3)
    - `dPixeldx`/`dPixeldy` (3 each), `projection`, `imagePlane` (2 each)
    - `toWorld`, `cameraHasRotation`, `inFrustrum`, `raytracingFlags` (1 each)
  - Locks: `CRenderer::tesselateMutex` (5), `CRenderer::delayedMutex` (4).
  - Upward calls into the scene context, for lazy instancing and procedurals:
    `CRenderer::context->processDelayedSolid` (2), `processDelayedObject`, `processDelayedInstance`.
  - Services: `CRenderer::retrieveVariable` (5), `locateFile`, `texturePath`, `environmentLoad`,
    `findCoordinateSystem`.
  - Related: `src/ri/render/geometryDispatch.cpp` implements Geometry methods (`CObject::dice`,
    `CSurface::dice`, `CDelayedObject::intersect`, and others) from the renderer directory.
  - Scene Interface: Geometry's base-class contract
    `CObject::instantiate(CAttributes *, CXform *, CRiInterface *)` (pure virtual, `object.h:77`)
    takes the Scene Interface class `CRiInterface`. 27 files under `src/ri/geometry/` reference it,
    and `riInterface.h` is included by `object.cpp`, `subdivisionCreator.cpp` and
    `subdivisionLoop.cpp`. The only use is reporting created objects through
    `CRiInterface::addObject()`: 25 call sites across 10 files (14 in `quadrics.cpp`).
- **Direction**: invert through interfaces Geometry owns and the renderer core implements:
  - a frame/camera-parameters interface (or pass these values through the dice/intersect context)
  - a host interface for locks and for capturing instantiated or expanded objects, replacing the
    `processDelayed*` calls
  - service interfaces for file, texture and coordinate-system lookups
  - a Geometry-owned object sink in `instantiate()` in place of `CRiInterface` (its only use is
    `addObject()`), implemented by the renderer core and the RIB/preview back-ends
- **Measured by the architecture check** (spec 022, macOS): 65 edges allowlisted under DB-004.
  - 28 includes: `renderer.h` (17), `rendererContext.h` (9), `riInterface.h` (2).
  - 37 link references to `CRenderer` members.
  - Geometry's other cross-domain edges are registered elsewhere, so they are not counted
    here: diagnostics, RI tokens and `rendererc.h` under DB-016 (44 edges); symbols defined by
    `geometryDispatch.cpp`/`geometryDispatchStub.cpp` under DB-017; Shading and Hiders
    coupling under DB-006; the data viewer under DB-025.
- **History**: spec 021 (deferred CSG) resolves solids and procedural operands through a new
  Geometry-owned host interface rather than extending `processDelayed*`/`delayedMutex`; the
  existing uses remain.

### DB-005 — Hiders and Shading share one class (`CShadingContext`)

- **Rules**: 2 (acyclic dependencies); the allowed-dependency table (Shading may not depend on
  Hiders).
- **Tracking**: remediation spec 029 ([#35](https://github.com/juvenal/openrender/issues/35))
- **Evidence**:
  - `CReyes` (`reyes.h:104`), `CRaytracer` (`raytracer.h:140`) and `CPhotonHider`
    (`photon.h:41`) inherit `CShadingContext`, which is declared in Shading
    (`src/libshader/shading/shading.h`).
  - Its trace bodies are defined in Hiders: `CShadingContext::trace` and `traceEx` in
    `src/ri/hiders/trace.cpp:47,240,289`.
  - Shading includes hider headers: `bundles.h` (5), `irradiance.h` (2), `photonMap.h` (2).
- **Size**: 9 include edges. The matching link references (trace, bundles, photon map) are
  counted under DB-001.
- **Direction**: composition. Shading's `ShadingContext` keeps state and execution with no hider
  knowledge, and is owned by Hiders' `CHider`; the hiders implement a Shading-owned `ITracer`.
  Strangler steps with byte-identical renders at each step.

### DB-006 — Geometry is coupled to Shading and to the hiders

- **Rules**: 2; the allowed-dependency table (Geometry may depend only on Graphics State and
  Geometric Procedures; Shading may not depend on Geometry).
- **Tracking**: remediation specs 026 ([#32](https://github.com/juvenal/openrender/issues/32)), 029 ([#35](https://github.com/juvenal/openrender/issues/35)), 031 ([#37](https://github.com/juvenal/openrender/issues/37))
- **Evidence**:
  - Geometry includes Shading headers 15 times (13 through the `src/ri/core/shading.h` forwarding
    shim) and calls `CShadingContext::shade`, `saveState` and `restoreState`.
  - Geometry includes `src/ri/hiders/reyes.h` from 11 files and calls `CReyes::drawObject` and
    `CReyes::drawGrid` (`surface.cpp`).
  - Shading includes `surface.h`, `object.h` and `points.h`, closing a Geometry ↔ Shading cycle.
- **Size**: 36 edges (29 include, 7 link).
- **Direction**: an OSL-style `ShaderGlobals` filled by Geometry for Shading (026, 029);
  Geometry's dice and intersect take Geometry-owned `IDicer`/`IRayContext` interfaces instead of
  `CReyes*`/`CShadingContext*` (031).

### DB-007 — The shader backend is chosen by concrete type inside the renderer core

- **Rules**: 3 (backend-neutral interfaces between domains); Shading owns shader loading.
- **Tracking**: remediation spec 033 ([#39](https://github.com/juvenal/openrender/issues/39))
- **Evidence**:
  - `parseShader`/`parseSloShader` and the `shaderformat` precedence live in
    `src/ri/render/rendererFiles.cpp:48,130,693-716`.
  - `CShader` carries both the bytecode and `jitEntry`
    (`src/libshader/shading/shader.h:157,223`); the runtime picks the backend by
    `jitEntry != nullptr`.
- **Size**: not a dependency edge (the renderer core may depend on Shading), so there are no
  allowlist lines; tracked by review.
- **Direction**: a `ShadingSystem` facade with `IShaderBackend` implementations (RSL interpreter,
  RSL JIT, later OSL). Loading, caching and format selection move into Shading; the core passes
  only a preference.

### DB-008 — Graphics State owns other domains' objects

- **Rules**: the allowed-dependency table (Graphics State depends on nothing).
- **Tracking**: remediation spec 025 ([#31](https://github.com/juvenal/openrender/issues/31))
- **Evidence**:
  - `attributes.h` and `options.h` include Shading's `shader.h` (directly and through the
    `src/ri/core/shader.h` shim) to hold `CShaderInstance*` slots.
  - `options.h` includes `src/display/dsply.h` (display function pointers) and
    `src/ri/geometry/blobbyField.h`.
  - `attributes.cpp` includes `src/ri/hiders/photonMap.h` (photon-map attach/detach).
- **Size**: 7 include edges. Graphics State's diagnostics and RI-token edges are under DB-016
  (81 edges); `options.cpp` → `texture.h` and `attributes.cpp` → `irradiance.h` are under DB-022.
- **Direction**: state stores opaque handles only (`ShaderGroupRef` with a `bxdf` slot, `MapRef`)
  plus a scoped pattern-node table; shader-parameter logic moves to Shading; a state-owned
  vocabulary header.

### DB-009 — Motion blur and depth of field are implemented more than once

- **Rules**: 3 (an effect is implemented once, generically, for every hider).
- **Tracking**: remediation specs 028 ([#34](https://github.com/juvenal/openrender/issues/34)), 031 ([#37](https://github.com/juvenal/openrender/issues/37))
- **Evidence**:
  - `CRenderer::invShutterTime` is read by 7 Geometry files (`quadrics.cpp`, `patches.cpp`,
    `patchgrid.cpp`, `bsplinePatchgrid.cpp`, `polygons.cpp`, `points.cpp`, `curves.cpp`) to
    interpolate their own motion at intersection time, separately from the dicing path.
  - Depth of field has two models: the REYES aperture jitter (`stochastic.h:81,108`) and the
    raytrace thin lens (`raytracer.cpp:612-621`).
- **Size**: not a dependency edge; tracked by review. (The `invShutterTime` reads themselves are
  link edges under DB-004.)
- **Direction**: one camera layer (`ICamera::generateRay`, `FrameParams`) and one Geometry-owned
  motion evaluator used by dicing and intersection alike.

### DB-010 — Texturing depends on Shading, the renderer core and Graphics State

- **Rules**: the allowed-dependency table (Texturing depends on nothing; Hiders may not depend on
  Texturing).
- **Tracking**: remediation specs 027 ([#33](https://github.com/juvenal/openrender/issues/33)), 032 ([#38](https://github.com/juvenal/openrender/issues/38))
- **Evidence**:
  - Lookups take `CShadingContext*` (`src/ri/core/shading.h` included by 2 Texturing files).
  - Texturing reads renderer-core state: `CRenderer::locateFile`, `texturePath`, `textureMutex`,
    `synthesizeMutex`, `textureMaxMemory`, `textureRefNumber`, `numThreads` (14 link edges), and
    includes `renderer.h`.
  - Texturing includes `options.h`, `attributes.h` and `xform.h` from Graphics State.
  - The hiders' GI caches use Texturing: `irradiance`/`photon` code includes `texture.h`,
    `texture3d.h`, `pointHierarchy.h`, `tiff.h` and constructs `CTexture3d`.
- **Size**: 36 edges (19 include, 17 link).
- **Direction**: a Texturing-owned `TextureSystem` (opaque handles, `TextureOpt`, per-thread
  data, cache and paging) that Shading reaches only through services (027); GI-cache storage
  moves to Texturing with sampling left in Hiders (032).

### DB-011 — Hiders read renderer-core state (`CRenderer`)

- **Rules**: the renderer-core rule (no domain may depend on the renderer core).
- **Tracking**: remediation specs 028 ([#34](https://github.com/juvenal/openrender/issues/34)), 029 ([#35](https://github.com/juvenal/openrender/issues/35)), 030 ([#36](https://github.com/juvenal/openrender/issues/36))
- **Evidence**:
  - 21 files under `src/ri/hiders/` make 611 `CRenderer::` references (grep count): frame and
    sampling parameters (`clipMax`, `bucketWidth`, `pixelXsamples`, `xSampleOffset`, …),
    `flags`, `hiderFlags`, `raytracingFlags`.
  - 9 files include `src/ri/render/renderer.h`.
- **Size**: 180 edges per platform (9 include, 171 link). The thread-local `CRenderer::activeContext`
  is referenced through Clang's TLS wrapper (`_ZTW…`) on macOS and directly by GCC on Linux, so its
  2 edges are listed once per platform, tagged (182 allowlist lines).
- **Direction**: inject immutable `FrameParams` and per-thread contexts; the mutable flags become
  capabilities on the injected interfaces. Each hider-side spec removes the statics it uses.

### DB-012 — The JIT ignores the hiders' illumination hook

- **Rules**: 3 (a shader behaves the same under every hider and backend).
- **Tracking**: remediation specs 024 ([#30](https://github.com/juvenal/openrender/issues/30)), 026 ([#32](https://github.com/juvenal/openrender/issues/32)), 029 ([#35](https://github.com/juvenal/openrender/issues/35))
- **Evidence**:
  - The interpreter checks `HIDER_ILLUMINATIONHOOK` in every illuminate/solar opcode
    (`src/libshader/shading/shaderOpcodes.h:214,267,319,376,413,464`).
  - The JIT emits `op_illuminate_begin`/`op_illuminate3_begin`/`op_solar_begin`
    (`llvmEmitter.cpp:1175,1186,1214`), which never check it.
  - The hook-aware `rsl_illuminate_begin_*`/`rsl_solar_begin_*` wrappers
    (`rslBuiltins.cpp:480,519,638,663`) have no callers.
- **Size**: not a dependency edge; tracked by review.
- **Direction**: the hook becomes an explicit capability on the shading services, honoured by
  both backends; a `.slo` vs `.rslo` photon-pass parity test (024).

### DB-013 — No hider registry: `strcmp` chains in the renderer core

- **Rules**: 3; adding a hider must not require core edits.
- **Tracking**: remediation spec 030 ([#36](https://github.com/juvenal/openrender/issues/36))
- **Evidence**:
  - Two `strcmp(hider, …)` chains in `src/ri/render/renderer.cpp:747-787`, plus the
    `"stochastic"` alias in `rendererContext.cpp:1529`.
  - The built-in `CSphereLight`/`CQuadLight` fallback (`src/ri/hcshader.{h,cpp}`, 776 lines) is
    still compiled, and both classes are friends of `CShadingContext` (`shading.h:903-904`).
- **Size**: not a dependency edge; tracked by review.
- **Direction**: a self-registering hider factory (name, aliases, capabilities); retire
  `hcshader` now that the RSL `spherelight`/`quadlight` ship.

### DB-014 — Plug-in ABIs are unsafe beyond versioning

- **Rules**: 4 (plug-in interfaces are versioned, C-includable and documented).
- **Tracking**: remediation spec 037 ([#26](https://github.com/juvenal/openrender/issues/26)); versioning itself is DB-003.
- **Evidence**:
  - `dsply.h` hands display drivers writable pointers to renderer-core statics
    (`src/ri/render/rendererDisplay.cpp:99-111`: `&CRenderer::clipMin`, `clipMax`,
    `fromWorld`, `worldToNDC`), and the current display is a file-static
    (`rendererDisplay.cpp:41`).
  - A shadeop's `init` runs once (`rendererFiles.cpp:960`, `init(0, NULL)`) and its data is
    shared by all threads; reentrancy and unload order are undocumented.
  - The `dlo`/`implicit`/`RiProcedural` loaders are spread over the renderer core, Geometry and
    Scene Interface.
- **Size**: not a dependency edge; tracked by review.
- **Direction**: per family, a versioned C header with a version query symbol (DB-003), a `const`
  per-instance context with channel descriptors, documented threading, per-thread shadeop init
  and deferred unload; one Geometric Procedures module owning the loaders.

### DB-015 — Scene Interface depends on the renderer core

- **Rules**: the renderer-core rule.
- **Tracking**: remediation spec 038 ([#43](https://github.com/juvenal/openrender/issues/43))
- **Evidence**:
  - The generated RIB, `ifexpr` and `variable` parsers include `renderer.h`/`rendererContext.h`
    and call `CRenderer::locateFile` and `retrieveVariable`.
  - `ri.cpp` calls `CRenderer::initDeclarations`/`shutdownDeclarations`.
  - `ri.cpp` and `ribOut.cpp` include `src/ri/render/riHooks.h`.
  - `CRiInterface`'s constructor installs itself as the global `renderMan`
    (`src/ri/parse/riInterface.cpp:36`).
- **Size**: 11 edges (7 include, 4 link).
- **Direction**: a Scene-owned declaration and file-search service implemented by the core;
  `riHooks.h` moves into Scene Interface; explicit active-context installation.

### DB-016 — Infrastructure lives in Scene Interface and the renderer core

- **Rules**: the allowed-dependency table (every domain may depend only on infrastructure for
  shared services); infrastructure may depend on no domain.
- **Tracking**: remediation spec 023 ([#29](https://github.com/juvenal/openrender/issues/29))
- **Evidence**:
  - Diagnostics (`error`/`warning`/`info`, `src/ri/parse/error.h`) are Scene Interface, used by
    every domain.
  - The RI token constants (`RI_*`, defined in `ri.cpp`) and the `ri.h` vocabulary are used by
    Graphics State (74 link references), Geometry and Texturing.
  - The renderer-core vocabulary `src/ri/render/rendererc.h` and `CVariable`/`parseVariable`
    (`src/ri/parse/variable.h`) are used across domains.
  - The global memory arena is a `CRenderer` static (`CRenderer::globalMemory`).
  - Infrastructure itself reaches up: `src/ri/core` and `src/common` include `error.h`, `ri.h`,
    `rendererc.h`, `attributes.h` and `xform.h`, and call `CAttributes::findParameter`,
    `CRenderer::retrieveVariable`.
- **Size**: 220 edges (80 include, 140 link) across Graphics State (81), Geometry (44), Texturing
  (36), Hiders (19), Infra (20), Shading (15) and Scene Interface (5). Shading's link
  references to these symbols are counted under DB-001.
- **Direction**: move diagnostics (an installable `IDiagnosticSink`), the vocabulary headers,
  `CVariable` and an `Arena` object into `libRIBCommon`; behavior-preserving moves with
  forwarding headers for one spec.

### DB-017 — Link-time `*Dispatch` / `*DispatchStub` substitution pairs

- **Rules**: 2 and 3 (dependencies go through interfaces, not through which file the linker
  receives).
- **Tracking**: remediation spec 031 ([#37](https://github.com/juvenal/openrender/issues/37))
- **Evidence**: four pairs, swapped by `src/ri/CMakeLists.txt` (lines 266-303) depending on
  the library being built: `render/geometryDispatch.cpp` / `vector/geometryDispatchStub.cpp`,
  `render/rendererDeclarationsDispatch.cpp` / `vector/rendererDeclarationsDispatchStub.cpp`,
  `geometry/surfaceCrossingDispatch.cpp` / `vector/surfaceCrossingDispatchStub.cpp` (spec 021),
  `hiders/irradianceDispatch.cpp` / `vector/irradianceDispatchStub.cpp`. Geometry methods
  (`CSurface::dice`, `CSurface::shade`, `CSurface::regionCoverage`, `CObject::dice`, the
  `CSurface` vtable and typeinfo) are defined in the renderer-core file.
- **Size**: 78 link edges (Geometry 70, Hiders 7, plus one `rendererDeclarations` pair edge). A
  wrong assumption only fails at run time.
- **Direction**: delete each pair once its consumer is interface-based (Geometry's `IDicer`/
  `IRayContext`, injected RNG and state notifications); `ribVector` must still link without the
  stubs.

### DB-018 — Message passing reads `CAttributes` directly

- **Rules**: 3 (Shading reaches other domains' data through services).
- **Tracking**: remediation specs 026 ([#32](https://github.com/juvenal/openrender/issues/32)), 033 ([#39](https://github.com/juvenal/openrender/issues/39)), 034 ([#40](https://github.com/juvenal/openrender/issues/40))
- **Evidence**: `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()`,
  `option()` and `attribute()` (`src/libshader/shading/shading.cpp:1291-1361,1490`) read fields of
  `currentShadingState->currentObject->attributes` themselves.
- **Size**: not a dependency edge (Shading may depend on Graphics State); tracked by review.
- **Direction**: `RendererServices::get_attribute`, used by both backends; `getmessage`/
  `setmessage` across shader-group layers (034).

### DB-019 — The shader compiler depends on Scene Interface and renderer-core headers

- **Rules**: the allowed-dependency table (Shading may not depend on Scene Interface or the
  renderer core).
- **Tracking**: remediation specs 038 ([#43](https://github.com/juvenal/openrender/issues/43)), 037 ([#26](https://github.com/juvenal/openrender/issues/26))
- **Evidence**:
  - `src/libshader/compiler/rslo.cpp:38` and the generated DSO and `rslo` parsers include
    `src/ri/parse/dso.h`.
  - `llvmEmitter.cpp` includes `src/ri/render/rendererc.h`.
  - The compiler `dlopen`s shadeop DSOs itself (`rslo.cpp:1701`), and the JIT has no DSO shadeop
    path.
- **Size**: 4 include edges.
- **Direction**: a standalone compiler with its own builtin and opcode tables, reading shadeop
  prototypes only through the versioned `shadeop.h`; DSO loading moves to the shadeop module
  (037); a JIT DSO call path.

### DB-020 — Cross-domain `friend` declarations

- **Rules**: 3 (no back doors into another domain's internals).
- **Tracking**: remediation specs 029 ([#35](https://github.com/juvenal/openrender/issues/35)), 030 ([#36](https://github.com/juvenal/openrender/issues/36)), 032 ([#38](https://github.com/juvenal/openrender/issues/38))
- **Evidence**:
  - `CShadingContext` befriends `CPhotonHider`, `CSphereLight` and `CQuadLight`
    (`shading.h:901,903,904`).
  - `CIrradianceCache` befriends `CRemoteICacheChannel` (`irradiance.h:137`); `CTexture3d` and
    `CPointCloud` befriend `CRemotePtCloudChannel` (`texture3d.h:100`, `pointCloud.h:149`).
  - `CBrickMap` befriends `CBrickMapGeometry` (`brickmap.h:254`), a class forward-declared at
    `brickmap.h:39` and defined nowhere.
- **Size**: not a dependency edge; tracked by review.
- **Direction**: narrow public accessors or interfaces, removed with the spec that touches each
  class; delete the dangling friend.

### DB-021 — Texturing reads the AOV/channel registry

- **Rules**: the allowed-dependency table (Texturing depends on nothing).
- **Tracking**: remediation specs 025 ([#31](https://github.com/juvenal/openrender/issues/31)), 037 ([#26](https://github.com/juvenal/openrender/issues/26))
- **Evidence**: `src/ri/texture/texture3d.cpp:29` includes `src/ri/state/displayChannel.h` and
  builds its channel list from the registry; the registry itself is renderer-core state.
- **Size**: 1 include edge.
- **Direction**: the registry is owned by Graphics State (025) and passed to its consumers as
  channel descriptors (037).

### DB-022 — Vestigial cross-domain includes

- **Rules**: the allowed-dependency table.
- **Tracking**: spec 022 (this spec removes them, Phase 6)
- **Evidence**: includes the audit found unused, still present and still violations:
  - `src/ri/state/options.cpp` → `texture.h`; `src/ri/state/attributes.cpp` → `irradiance.h`;
  - `src/libshader/shading/shading.cpp` → `raytracer.h`, `remoteChannel.h`, `points.h`;
  - `src/ri/texture/pointCloud.cpp` → `object.h`, `raytracer.h`.

  The audit's other three candidates are not violations: `shading.cpp` → `brickmap.h` and
  `pointCloud.h` (Shading → Texturing) and `reyes.cpp` → `points.h` (Hiders → Geometry) are
  allowed.
- **Size**: 7 include edges.
- **Direction**: delete each one, with a clean `-Werror` build after each removal; any that turns
  out to be needed moves to the entry that owns its dependency.

### DB-023 — Domains are not build targets

- **Rules**: 1 (each domain is a separately buildable unit).
- **Tracking**: spec 022 adds the check; every remediation spec 023–038 narrows include paths for
  its domain ([#29](https://github.com/juvenal/openrender/issues/29)–[#43](https://github.com/juvenal/openrender/issues/43), and [#26](https://github.com/juvenal/openrender/issues/26) for 037).
- **Evidence**:
  - `ribRender_obj` (`src/ri/CMakeLists.txt:178`) compiles sources from six domains plus the
    renderer core into one OBJECT library.
  - `libshader_shading`'s include path lists `src/ri/{core,dataviewer,geometry,hiders,output,
    parse,render,state,texture}`, so nothing stops a new cross-domain include at compile time.
- **Size**: not a dependency edge; tracked by review and by the architecture check.
- **Direction**: per-domain include visibility first, then per-domain OBJECT targets as each layer
  lands.

### DB-024 — Tools reach into domain internals

- **Rules**: none broken (consumers may depend on anything). Registered as evidence for the
  public APIs the tools should use.
- **Tracking**: remediation specs 033 ([#39](https://github.com/juvenal/openrender/issues/39)), 037 ([#26](https://github.com/juvenal/openrender/issues/26)), 038 ([#43](https://github.com/juvenal/openrender/issues/43))
- **Evidence** (the check's `CONSUMER` section, 36 include edges):
  - `sloinfo.cpp` and `oshader.cpp` include Shading internals (3 each);
  - `precomp.cpp` includes a hider header;
  - `libribpreview` includes 15 Geometry headers, 3 Graphics State headers and 4 Scene Interface
    headers;
  - `src/ri/dataviewer/dataLoad.cpp` reaches into Hiders and Texturing.
- **Size**: 36 consumer-reach edges; reported, never failing, no allowlist lines.
- **Direction**: tools use public APIs: a shader-metadata API from Shading (033), one display ABI
  definition (037), `RiMake*` for precomputation (038).

### DB-025 — Domains call into the data viewer (a consumer)

- **Rules**: the allowed-dependency table (no domain may depend on a consumer).
- **Tracking**: remediation spec 032 ([#38](https://github.com/juvenal/openrender/issues/38))
- **Evidence**: Texturing (`pointCloud.cpp`, `brickmap.cpp`, `texture3d.h`) and Hiders
  (`irradiance.cpp`, `irradianceDispatch.cpp`, `photonMap.{h,cpp}`) include
  `src/ri/dataviewer/dataView.h`/`debug.h` and call `CDataView::drawPoints`, `drawDisks` and
  `drawTriangles`; Geometry's `surface.cpp` includes `debug.h`. Found by the check; not one of
  the audit's V1–V21.
- **Size**: 13 edges (5 include, 8 link).
- **Direction**: the data viewer reads the GI and texture data through adapters it owns
  (spec 032's `CDataView` adapters), so the domains no longer know it exists.

### DB-026 — Texturing uses the RGBE codec in Display Outputs

- **Rules**: the allowed-dependency table (Texturing depends on nothing).
- **Tracking**: remediation spec 027 ([#33](https://github.com/juvenal/openrender/issues/33))
- **Evidence**: spec 018's RGBE decoder, `src/ri/texture/imageInputRgbe.cpp`, includes
  `src/display/rgbe/rgbe.h` and calls `RGBE_ReadHeader`/`RGBE_ReadPixels`, which are defined in
  the Display Outputs domain. Found by the check; not one of the audit's V1–V21.
- **Size**: 3 edges per platform (1 include, 2 link). `FILE*` mangles as `__sFILE` on macOS and
  `_IO_FILE` on glibc, so the 2 link edges are listed once per platform, tagged (5 allowlist lines).
- **Direction**: move the RGBE codec into infrastructure (`src/common`), shared by the display
  driver and the texture decoder.

## Resolved

_None yet._
