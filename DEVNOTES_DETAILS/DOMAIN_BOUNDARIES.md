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
- When an entry is fully resolved, move it to **Resolved** with the resolving spec (or `fix/` branch) and the date — don't delete it. Record nothing that only exists after the merge, such as the merge commit: `git log --merges` finds it from the spec number.
- Only record what the code demonstrably shows (a symbol list, a header, a link line), not suspicion.

## Architecture check

`archcheck` (spec 022, `tests/architecture/`) enforces this registry. It maps every source under
`src/` to its domain and reads the build tree for two kinds of dependency edge:
- **include edges**: each file's direct `#include`s, resolved the way its compiler resolved them
  (the search path from `flags.make`, confirmed by the translation unit's depfile, so includes in
  inactive `#if` branches drop out);
- **link edges**: an undefined symbol in a source file's object, attributed to the classes whose
  sources define it (`nm`).

An edge outside the constitution's allowed-dependency table fails the check unless the allowlist
names it against an **Open** entry below. The allowlist only shrinks: an entry whose edge is gone
fails as stale.

**Run it**

- `ctest --test-dir build -L architecture`: the real-tree check (`Architecture_DomainBoundaries`),
  the fixture scenarios, the parser tests and the determinism test. Needs a complete build.
- Directly, for the full report:
  `build/tests/architecture/archcheck --source-dir . --build-dir build --config tests/architecture`
  (`--format json`, `--quiet`, `--platform macos|linux`, `--emit-allowlist`; see `--help`).
- Exit codes: 0 pass, 1 check failed, 2 configuration or environment error (unclassified file,
  malformed data file, incomplete build tree, missing `nm`).

**Read the report**

| Line | Meaning | Fails? |
|---|---|---|
| `VIOLATION` | an edge the table forbids and no allowlist line covers: `file:line` of the include (or the referencing file and symbol, raw and demangled, with its defining file), both classes, the rule | yes |
| `STALE` | an allowlist line whose edge no longer exists, or is now allowed: delete the line | yes |
| `BADID` | an allowlist line naming an unknown or Resolved registry ID | yes |
| `CYCLE` | the shortest cycle through each disallowed class-to-class step (not every cycle), with one example edge per step; `[FAILING]` if a step holds a violation, `[allowlisted]` otherwise | if `[FAILING]` |
| `ALLOWED` | allowlisted edges per registry ID | no |
| `CONSUMER` | a tool, binding or preview including a domain's headers (recorded under DB-024) | no |
| `NOTBUILT` | an allowlist line for a file this build does not compile, or an include line whose `#include` is still written but inactive in this build (e.g. inside an `#ifdef` the build options leave off) | no |
| `UNRESOLVED` | a header no compiled translation unit includes, so it yields no edges | no |
| `METRIC` / `WARNING` / `NOTE` | renderer-core usage (`CRenderer::` / `CRendererContext::` references and distinct members) per class against the baseline; an increase warns, a decrease suggests lowering the baseline | no |

**Data files** (`tests/architecture/`, formats in
`specs/022-domain-dependency-guardrails/contracts/data-files.md`)

- `domain-map.txt`: path prefix or exact file → class. Classification is by location; file rules
  exist only for forwarding shims, plug-in ABI headers and generated sources.
- `domain-rules.txt`: the constitution's allowed-dependency table. Change only by amendment.
- `allowlist.txt`: one line per known violating edge, with its registry ID and an optional
  `macos`/`linux` tag for edges the two toolchains spell differently.
- `metrics-baseline.txt`: renderer-core usage per class.

**Classify a new file** (the check stops with `unclassified: <path>`)

1. Find the domain that owns the code (constitution Principle VIII). A file in an existing domain
   directory is already classified; this applies only to a new directory or a misplaced file.
2. Prefer moving the file into its domain's directory over adding a rule.
3. Otherwise add one `prefix` line for the new directory (or a `file` line for a single file) to
   `domain-map.txt`.
4. Rebuild and run `ctest -L architecture`; fix or register any `VIOLATION` it now reports.

**Retire a fixed violation**

1. Make the fix and rebuild; `ctest -L architecture` reports the now-missing edges as `STALE`.
2. Delete exactly those lines from `allowlist.txt`.
3. If the entry has no allowlist lines left and its finding is gone, move it under **Resolved**
   with the resolving spec and the date.
4. If `NOTE metric … < baseline` appears, lower `metrics-baseline.txt` to the reported values.
5. Re-run `ctest -L architecture` on macOS and Linux.

**Build options:** an allowlisted include inside an `#if` that a configuration leaves off is reported as `NOTBUILT` there, not `STALE`. Link entries have no such rule (`nm` can't tell an inactive block from deleted code): a link entry whose reference exists only under a build option needs a platform tag, or that option on in every configuration that runs the check. None is known today.

**Known limitation:** CI runs the check only where `ctest` runs, i.e. in
`.github/workflows/release.yml`, which runs on version-tag pushes, not on every push or pull
request. Run `ctest -L architecture` locally before review.

## Open

### DB-001 — Shading resolves renderer-core symbols at load time

- **Rules**: 2 (acyclic dependencies; no upward dependency via load-time-bound symbols), and the
  renderer-core rule (no domain may depend on the renderer core).
- **Tracking**: [#24](https://github.com/juvenal/openrender/issues/24) (enhancement / design review)
- **Evidence**: `libshader_shading` (Shading) links with `-undefined dynamic_lookup` (macOS) /
  `--allow-shlib-undefined` (Linux) in `src/libshader/shading/CMakeLists.txt`, and binds 7
  distinct `libRIBRender` symbols flat-namespace (`nm -m`, "dynamically looked up"):
  - Hiders: `CShadingContext::trace`/`traceEx`, `CGatherBundle`, `CTraceBundle`/
    `CTransmissionBundle` vtables, `CPhotonMap::lookup`;
  - Texturing: `CTexture3d::resolve`.
- **History**: before spec 023 it bound 29 symbols. The 17 infrastructure ones (`CRenderer::
  globalMemory`, `CRefCounter`, `CFileResource`, `memoryInit`/`memoryTini`/`memoryNewPage`,
  `stats`, `error`/`info`/`warning`, `parseVariable`, `sampleHemisphere`/
  `sampleCosineHemisphere`, ...) now resolve from `libRIBCommon` (DB-016, Resolved). The record
  is `specs/023-common-infrastructure-floor/tools/libshader-nm-m-{before,after}-macos.txt`.
  `parseVariable` is defined in `libRIBCommon`, so spec 024 needed no variable-parsing service.
  - Spec 024, 2026-10-16: the five renderer-core lookups (`CRenderer::getDSO`, `getFilter`,
    `getStepFilter`, `getGlobalID`, `retrieveVariable`) go through `CRendererServices`
    (`getDSO` added to it), reached by Shading through the helpers in `serviceLookup.h`;
    12 → 7 imports, 18 → 8 edges. The record is
    `specs/024-shading-services-routing/tools/libshader-dynlookup-{before,after}-macos.txt`.
    `CDSO` moved to `src/libshader/shading/dsoHandle.h`, and no Shading source includes
    `renderer.h` any more.
- **Measured by the architecture check** (macOS): 8 edges allowlisted under DB-001 (18 before
  spec 024, 30 before spec 023).
  - 8 link references from Shading sources: `CShadingContext::trace`/`traceEx`, `CGatherBundle`,
    the `CTraceBundle`/`CTransmissionBundle` vtables and `CPhotonMap::lookup`.
  - No includes: the 4 of `src/ri/render/renderer.h` are gone.
  - Not edges: `CTexture3d::resolve` is Shading → Texturing, which the table allows (a
    load-time binding problem only).
  - Shading's source-level coupling is registered separately: hider headers under DB-005,
    Geometry headers under DB-006, and `dso.h` under DB-019.
- **Consequences today**: `libshader_shading` cannot load without `libRIBRender` in the process; a
  static engine link must whole-archive `libRIBRender.a` (the `openRender::RIBRender_static`
  package target applies `-force_load` for this — remove that workaround once this is resolved).
- **Direction**: generic infrastructure is already in `libRIBCommon` (spec 023) and the
  renderer-core lookups go through `CRendererServices` (spec 024); invert the remaining
  Hiders/Texturing calls through interfaces Shading owns (trace/gather, photon/texture3d lookup),
  implemented and registered by the higher layers. Options are analysed in #24.

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
    here: diagnostics, RI tokens and `rendererc.h` were under DB-016 (41 edges, resolved by
    spec 023); symbols defined by
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
  - Shading includes hider headers: `bundles.h` (4), `irradiance.h` (2), `photonMap.h` (2).
- **Size**: 8 include edges. The matching link references (trace, bundles, photon map) are
  counted under DB-001.
- **Direction**: composition. Shading's `ShadingContext` keeps state and execution with no hider
  knowledge, and is owned by Hiders' `CHider`; the hiders implement a Shading-owned `ITracer`.
  Strangler steps with byte-identical renders at each step.
- **History**: the per-context RNG state is gone, the RNG part of V1 (spec 039, 2026-10-20):
  `CShadingContext` no longer holds a generator (the Mersenne Twister, `urand()`/`irand()` and the
  `random2d/3d/4d` CSobol members were deleted); every draw comes from the sampler's sub-stream
  of the sample's identity, so the shared class no longer carries random state between the
  hider and the shader it runs. The class sharing itself (this entry) is unchanged.

### DB-006 — Geometry is coupled to Shading and to the hiders

- **Rules**: 2; the allowed-dependency table (Geometry may depend only on Graphics State and
  Geometric Procedures; Shading may not depend on Geometry).
- **Tracking**: remediation specs 026 ([#32](https://github.com/juvenal/openrender/issues/32)), 029 ([#35](https://github.com/juvenal/openrender/issues/35)), 031 ([#37](https://github.com/juvenal/openrender/issues/37))
- **Evidence**:
  - Geometry includes Shading headers 15 times (13 through the `src/ri/core/shading.h` forwarding
    shim) and calls `CShadingContext::shade`, `saveState` and `restoreState`.
  - Geometry includes `src/ri/hiders/reyes.h` from 9 files and calls `CReyes::drawObject` and
    `CReyes::drawGrid` (`surface.cpp`).
  - Shading includes `surface.h` and `object.h`, closing a Geometry ↔ Shading cycle.
- **Size**: 33 edges (26 include, 7 link).
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
    `texture3d.h` and `pointHierarchy.h` and constructs `CTexture3d`.
  - Since spec 025 (2026-10-27), `CRenderer::retrieveDisplayChannel`, which `texture3d.cpp` calls,
    forwards to the Graphics State-owned channel registry. It is still defined out of line in the
    renderer core, so the link edge is unchanged.
- **Size**: 35 edges (18 include, 17 link).
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
- **History**: spec 039, 2026-10-20: the hiders take every camera value from the sampler's
  camera slots through an immutable per-frame `CSamplingConfig`, so the Legacy camera path's
  reads went with it. Deleted allowlist lines: `raytracer.cpp` and `stochastic.cpp ->
  CRenderer::correlatedSampleTable` (the correlated table, deleted), `stochastic.cpp ->
  CRenderer::frame` (the seed of the deleted `apertureGenerator`), and `raytracer.cpp` and
  `stochastic.cpp -> CRenderer::jitter`. The Hiders `CRenderer::` metric fell from 586 to 567
  references and 87 to 84 members.

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
- **History**: spec 025 (2026-10-27): an attribute block's active-light list holds non-owning
  `CShaderGroupRefTarget *` entries. The unchecked cast of an entry to a
  `CProgrammableShaderInstance *` (the built-in `CSphereLight`/`CQuadLight` are not programmable
  instances; the consumers read only `CShaderInstance` members) now lives in `lightInstanceOf()`
  (`src/libshader/shading/shaderRefs.h`), preserved as it was.

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
  - The generated RIB and `ifexpr` parsers include `renderer.h`/`rendererContext.h`, and the
    RIB parser calls `CRenderer::locateFile` and `retrieveVariable`. (The `variable` parser moved
    to `libRIBCommon` in spec 023 and no longer includes `renderer.h`.)
  - `ri.cpp` calls `CRenderer::initDeclarations`/`shutdownDeclarations`.
  - `ri.cpp` and `ribOut.cpp` include `src/ri/render/riHooks.h`.
  - `CRiInterface`'s constructor installs itself as the global `renderMan`
    (`src/ri/parse/riInterface.cpp:36`).
- **Size**: 10 edges (6 include, 4 link); 11 before spec 023.
- **Direction**: a Scene-owned declaration and file-search service implemented by the core;
  `riHooks.h` moves into Scene Interface; explicit active-context installation.

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
  stubs. The "injected RNG" part is done for `CObject::cluster()` (spec 039, 2026-10-20): its draws
  come from a `CKeyedStream` keyed by the object's `stableId` and the iteration, not from the
  shading context's generator; the pair itself remains for the other reasons above.

### DB-018 — Message passing reads `CAttributes` directly

- **Rules**: 3 (Shading reaches other domains' data through services).
- **Tracking**: remediation specs 026 ([#32](https://github.com/juvenal/openrender/issues/32)), 033 ([#39](https://github.com/juvenal/openrender/issues/39)), 034 ([#40](https://github.com/juvenal/openrender/issues/40))
- **Evidence**: `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()`,
  `option()` and `attribute()` (`src/libshader/shading/shading.cpp:1291-1361,1490`) read fields of
  `currentShadingState->currentObject->attributes` themselves.
- **Size**: not a dependency edge (Shading may depend on Graphics State); tracked by review.
- **Direction**: `RendererServices::get_attribute`, used by both backends; an explicit ray type /
  hit mode in `ShaderGlobals` (spec 026, [#32](https://github.com/juvenal/openrender/issues/32)),
  formerly listed under DB-012; `getmessage`/`setmessage` across shader-group layers (034).

### DB-019 — The shader compiler depends on Scene Interface and renderer-core headers

- **Rules**: the allowed-dependency table (Shading may not depend on Scene Interface or the
  renderer core).
- **Tracking**: remediation specs 038 ([#43](https://github.com/juvenal/openrender/issues/43)), 037 ([#26](https://github.com/juvenal/openrender/issues/26))
- **Evidence**:
  - `src/libshader/compiler/rslo.cpp:38` and the generated DSO and `rslo` parsers include
    `src/ri/parse/dso.h`.
  - The compiler `dlopen`s shadeop DSOs itself (`rslo.cpp:1701`), and the JIT has no DSO shadeop
    path.
- **Size**: 3 include edges; 4 before spec 023, which moved `rendererc.h` into `src/common`
  and so removed `llvmEmitter.cpp`'s include of it.
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
- **Tracking**: remediation spec 037 ([#26](https://github.com/juvenal/openrender/issues/26))
- **Evidence**: `src/ri/texture/texture3d.cpp:29` includes `src/ri/state/displayChannel.h` and
  builds its channel list by calling `CRenderer::retrieveDisplayChannel`, an out-of-line forwarder
  in the renderer core that, since spec 025 (2026-10-27), forwards to the Graphics State-owned
  `CChannelRegistry` (`src/ri/state/channelRegistry.{h,cpp}`). Texturing itself still reads the
  registry through the renderer core (the DB-010 link edge) and still includes the channel type.
- **Size**: 1 include edge.
- **Direction**: the registry is owned by Graphics State (done in 025) and passed to its consumers
  as channel descriptors (037), so that Texturing no longer names `displayChannel.h` or calls into
  the renderer core for it.
- **History**: spec 025 moved the registry out of the renderer core's statics into Graphics State.
  `texture3d.cpp` was deliberately left untouched, and the forwarder was kept out of line so that
  `texture3d.o` references no Graphics State symbol: that edge and the DB-010 link line are
  unchanged.

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
  (`irradiance.cpp`, `photonMap.{h,cpp}`) include
  `src/ri/dataviewer/dataView.h`/`debug.h` and call `CDataView::drawPoints`, `drawDisks` and
  `drawTriangles`; Geometry's `surface.cpp` includes `debug.h`. Found by the check; not one of
  the audit's V1–V21.
- **Size**: 12 edges (4 include, 8 link).
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
- **Resolved** by spec 022 (domain-dependency guardrails), 2026-10-07; merged into master by
  fast-forward up to `6870d01` (2026-10-08). `ctest -L architecture` runs `archcheck` (see **Architecture check** above) wherever
  `ctest` runs, including all three jobs of `.github/workflows/release.yml`. The direction above
  was taken with two refinements: edges come from compiler depfiles and per-object `nm` rather than
  `cmake --graphviz`, and every current violation (DB-001 and DB-004 included) is allowlisted per
  edge instead of being exempted as a whole. #25 is closed after the merge.

### DB-022 — Vestigial cross-domain includes

- **Rules**: the allowed-dependency table.
- **Tracking**: spec 022 Phase 6 (T051–T056)
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
- **Resolved** 2026-10-07 by spec 022 Phase 6:
  - all 7 edges removed (T051–T055), each with a clean `-Werror` build, and their allowlist lines
    deleted; the full suite is unchanged;
  - the audit's three allowed-but-unused candidates were removed too (`shading.cpp` →
    `brickmap.h`, `pointCloud.h`; `reyes.cpp` → `points.h`);
  - T056 tried 14 further cross-domain includes whose target declares nothing the including file
    names: 9 were unused and are removed (`shader.cpp` → `bundles.h`; `delayed.cpp`, `object.cpp`
    → `reyes.h`; `photonMap.cpp` → `tiff.h`; `csgDefinition.cpp`, `object.cpp`,
    `surfaceCrossingDispatch.cpp`, `reyes.cpp` → `rendererc.h`; `irradianceDispatch.cpp` →
    `debug.h`); their edges belonged to DB-005, DB-006, DB-010, DB-016 and DB-025, whose sizes
    above are updated. The other 5 are needed and stay registered under their entries.

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
- **Size**: 215 edges (75 include, 140 link) across Graphics State (81), Geometry (41), Texturing
  (36), Hiders (18), Infra (20), Shading (14) and Scene Interface (5). Shading's link
  references to these symbols are counted under DB-001.
- **Direction**: move diagnostics (an installable diagnostic sink), the vocabulary headers,
  `CVariable` and an arena object into `libRIBCommon`, as behavior-preserving moves. No
  forwarding headers: every include and call site switches to the common headers in the same
  spec.
- **Resolved** by spec 023 (common infrastructure floor, `specs/023-common-infrastructure-floor/`),
  2026-10-13.
  - `libRIBCommon` is now a shared library (plus the static archive) and the only definer of
    process-wide infrastructure state; the RI libraries re-export it, so existing consumers
    link and run unchanged.
  - Diagnostics: `CDiagnosticSink`/`installDiagnosticSink()` (`src/common/diagnostics.h`); Scene
    Interface installs the RI error handler, and with no RI context `error()` prints to stderr
    instead of dereferencing a null `renderMan`.
  - Vocabulary: `src/common/rendererc.h`, the RI tokens in `src/common/riVocabulary.h` (defined
    once in `riTokens.cpp`; `ri.h` is unchanged and `Common_VocabularyConsistency` keeps the
    lists identical), `CVariable` and the `variable` grammar in `src/common`.
  - Memory: `CArena globalArena` (`src/common/memory.h`) replaces `CRenderer::globalMemory`.
  - `CRefCounter`, `CFileResource`, stats and the sampling helpers moved from `src/ri/core`.
  - `pl.{h,cpp}` moved to Graphics State and reaches `CRenderer::retrieveVariable` through an
    installed lookup hook (`src/ri/state/variableLookup.h`).
  - All 215 DB-016 allowlist lines deleted, plus one line each under DB-015 and DB-019;
    the Infra `CRenderer::` metric is 0. `libshader_shading`'s flat-namespace imports fell from
    29 to 12.
  - Follow-up: the shared `globalArena`/`stats` become injected per-context state in specs 029
    and 039 (constitution rule 1). Split as done: spec 039 (2026-10-20) removed randomness from
    per-context state (draws are keyed by sample identity, no generator remains in a context);
    the arena and stats injection stays with spec 029.

### DB-012 — The JIT ignores the hiders' illumination hook

- **Rules**: 3 (a shader behaves the same under every hider and backend).
- **Tracking**: remediation spec 024 ([#30](https://github.com/juvenal/openrender/issues/30))
- **Evidence**:
  - The interpreter checked `HIDER_ILLUMINATIONHOOK` in every illuminate/solar opcode
    (`src/libshader/shading/shaderOpcodes.h:214,267,319,376,413,464`).
  - The JIT emitted `op_illuminate_begin`/`op_illuminate3_begin`/`op_solar_begin`
    (`llvmEmitter.cpp:1175,1186,1214`), which never checked it; under the photon hider, which
    leaves `costheta` unset, the photon pass crashed in `jitIlluminateBegin`.
  - A no-argument `solar()` emitted no begin call at all, so `L` was never set.
  - The hook-aware `rsl_illuminate_begin_*`/`rsl_solar_begin_*` wrappers
    (`rslBuiltins.cpp:480,519,638,663`) had no callers.
- **Size**: not a dependency edge; tracked by review.
- **Direction**: the hook becomes an explicit capability on the shading services, honoured by
  both backends; a `.slo` vs `.rslo` photon-pass parity test (024).
- **Resolved** by spec 024 (`specs/024-shading-services-routing/`), 2026-10-16:
  - one hook query for both backends, `CShadingContext::rendererIlluminationHook()` (backed by
    `CRendererServices::hasIlluminationHook()`), evaluated at every begin and end and never cached;
  - the JIT begin and end functions (`jitIlluminateBegin`, `jitIlluminate3Begin`, `jitSolarBegin`,
    `jitIlluminateEnd`, `jitSolarEnd`) hand off to the hider's hooks and touch neither `Ns`,
    `costheta`, the tags, the counts nor `L`;
  - the new `op_solar0_begin` (`jitSolar0Begin`) for `solar()` with no arguments, and a hard
    emitter error, naming the mnemonic, for `illuminate`/`solar` operands that resolve to none of
    their forms;
  - the six uncalled `rsl_illuminate_*`/`rsl_solar_*` wrappers and the unused
    `CRendererServices::hiderFlags()` deleted;
  - `Visual_photon-parity` (interpreter reference) and `Visual_photon-parity-slo`,
    `Visual_solar0{,-slo}`, `ShadingParity_IlluminationHook` and `LibShader_IlluminationOperands`
    guard it.
  - The explicit ray type / hit mode (spec 026, #32) moved to DB-018's Direction, and spec 029
    (#35) is already tracked under DB-005, so neither is lost by dropping them from this entry.

### DB-008 — Graphics State owns other domains' objects

- **Rules**: the allowed-dependency table (Graphics State depends on nothing).
- **Tracking**: remediation spec 025 ([#31](https://github.com/juvenal/openrender/issues/31))
- **Evidence**:
  - `attributes.h` and `options.h` include Shading's `shader.h` (directly and through the
    `src/ri/core/shader.h` shim) to hold `CShaderInstance*` slots.
  - `options.h` includes `src/display/dsply.h` (display function pointers) and
    `src/ri/geometry/blobbyField.h`.
  - `attributes.cpp` includes `src/ri/hiders/photonMap.h` (photon-map attach/detach).
- **Size**: 7 include edges. Graphics State's diagnostics and RI-token edges were under DB-016
  (81 edges, resolved by spec 023). `options.cpp` → `texture.h` and `attributes.cpp` → `irradiance.h` were unused and are removed (DB-022, Resolved).
- **Direction**: state stores opaque handles only (`ShaderGroupRef` with a `bxdf` slot, `MapRef`)
  plus a scoped pattern-node table; shader-parameter logic moves to Shading; a state-owned
  vocabulary header.
- **Resolved** by spec 025 (`specs/025-graphics-state-handles/`), 2026-10-27:
  - 7 include edges to 0, with the 7 allowlist lines deleted and nothing added or swapped;
  - `ShaderGroupRef` and `MapRef` (`src/ri/state/stateHandles.h`): reference-counted handles to
    `CShaderGroupRefTarget` (the base of `CShaderInstance`) and `CMapRefTarget` (a base of
    `CPhotonMap`), resolved only by the owning domain's inline `static_cast` (`shaderInstanceOf`,
    `lightInstanceOf` in `shaderRefs.h`; `photonMapOf` in `photonMap.h`); the shader slots are
    private, bound through `bind*()`, and a `bxdf` slot with the Surface/Bxdf last-bound record
    exists (no RIB statement); a scoped, copy-on-write pattern-node table (`patternTable.h`);
  - the used-parameter union and the parameter lookup moved to Shading
    (`libshader::usedShaderParameters` / `findShaderParameter`, `shaderParameters.cpp`) behind the
    State-owned function-pointer hook `shaderParameterHooks.h`, installed in
    `rendererShadingServicesInit()`;
  - `stateVocabulary.h` (`EDisplayParameterType`, `EBlobbyOrder`), kept equal to `dsply.h` and
    `blobbyField.h` by `State_VocabularyConsistency`; neither header changed;
  - the AOV/channel registry moved from the renderer core's statics into State
    (`CChannelRegistry`); DB-021 stays Open;
  - `CAttributes` copies and `restore()` keep the active lights in RIB order (they used to reverse
    them); no 8-bit image and no `MaxBlockAvgDiff` changed;
  - `State_Isolation`, `State_LastBound`, `State_PatternTable`, `State_ChannelRegistry`,
    `State_LightOrder`, `State_LightOrderRestore` and `State_VocabularyConsistency` guard it.
