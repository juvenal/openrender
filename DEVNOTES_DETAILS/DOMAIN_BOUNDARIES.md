# Domain Boundaries — Violation Registry

This file is the registry that constitution **Principle VIII (Domain Separation and Modular
Boundaries)** requires (rule 8). The domains, the allowed-dependency table and the rules themselves
live in `.specify/memory/constitution.md` (v1.3.0+); this file only tracks where the codebase does
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

**Checks of spec 038** (label `architecture`; each is a `cmake -P` script that reads the build system's
own view of a target, so none is a hand-kept list; they print `FAIL: <what>` lines and a count):

| Test | Fails when |
|---|---|
| `SceneInterface_RibOutNoCore` | a source of `libRIBOut` (`ribOut_obj`) is classified Core by `domain-map.txt` |
| `SceneInterface_NoCoreInclude` | a Scene Interface source (`src/ri/parse/`, `src/ri/output/`, a generated parser) includes a Core file |
| `SceneInterface_HooksAndRecords` | `riHooks.h` is not under `src/ri/parse/`, `CDelayedData` is defined outside the Scene Interface, or a Scene source includes `geometry/delayed.h` |
| `SceneInterface_ActiveContextAssignments` | a source under `src/` or `tests/` outside `src/ri/parse/` assigns `renderMan` or names `savedRenderMan` |
| `SceneInterface_RibVectorResidual` | `libRIBVector` (`ribVector_obj`) compiles a Core file `tests/architecture/ribvector-core-residual.txt` does not list, the list names a file it no longer compiles, or a "needed by" cell names a file under `parse/`, `output/` or `preview/` |
| `Preview_NoCoreReach` | a `libribpreview` source includes a Core header, or an object of it has an undefined `CRenderer`/`CRendererContext` symbol |
| `Compiler_IncludeBoundary` | the shader compiler's include path contains a Scene Interface, renderer or shading-engine directory, or a compiler source includes a file outside its allowed roots |
| `Precomp_NoHiderCore` | `precomp.cpp` includes `reyes.h` or a Core header |

Read a `SceneInterface_RibVectorResidual` failure by its text: "compiles the Core file X, which the
list does not list" means a new Core dependency of Geometry or Hiders (fix the dependency, or list the
file with the file that needs it, never with a Scene Interface or preview file); "lists X, which
ribVector_obj no longer compiles" means the residual shrank: delete the line. The list only shrinks;
`specs/038-scene-interface-untangle/tools/ribvector_core_sweep.sh <build-dir>` prints, from the built
objects, which Core file each `ribVector` object needs (an `nm` sweep; remove the objects of a source
the target stopped compiling first, CMake leaves them behind).

**Data files** (`tests/architecture/`, formats in
`specs/022-domain-dependency-guardrails/contracts/data-files.md`)

- `domain-map.txt`: path prefix or exact file → class. Classification is by location; file rules
  exist only for forwarding shims, plug-in ABI headers and generated sources.
- `domain-rules.txt`: the constitution's allowed-dependency table. Change only by amendment.
- `allowlist.txt`: one line per known violating edge, with its registry ID and an optional
  `macos`/`linux` tag for edges the two toolchains spell differently.
- `metrics-baseline.txt`: renderer-core usage per class.
- `ribvector-core-residual.txt`: the Core files `libRIBVector` still compiles and the Geometry or Hiders file
  that needs each (spec 038); read by `SceneInterface_RibVectorResidual`.

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

### DB-004 — Geometry depends on the renderer core and on Scene Interface

- **Rule**: the renderer-core rule ("no domain MAY depend on the renderer core"); the
  allowed-dependency table (Geometry may depend only on Graphics State and Geometric Procedures, not
  on Scene Interface); and 2 (an upward capability must be inverted through an interface the lower
  domain owns).
- **Tracking**: [#27](https://github.com/juvenal/openrender/issues/27)
- **Evidence**:
  - 17 files under `src/ri/geometry/` include `renderer.h`/`rendererContext.h` and use 16 distinct
    renderer-core symbols (`grep` over `src/ri/geometry/*.{h,cpp}` at `e83d005d`; spec 031 corrected the earlier 19 files and 24 symbols).
  - Frame and camera parameters (after spec 028; `invShutterTime` (24 uses), `relRotQ` (4),
    `toWorld` and `cameraHasRotation` are gone from this list):
    - `CRenderer::numThreads` (9), `maxGridSize` (9), `maxEyeSplits` (3), `flags` (3)
    - `dPixeldx`/`dPixeldy` (3 each), `projection`, `imagePlane` (2 each)
    - `inFrustrum`, `raytracingFlags` (1 each), and the dicing reads `pixelLeft`, `pixelTop`,
      `invFocaldistance`, `cocFactorPixels` (spec 031's)
  - Locks: `CRenderer::tesselateMutex` (5), `CRenderer::delayedMutex` (4).
  - Upward calls into the scene context, for lazy instancing and procedurals:
    `CRenderer::context->processDelayedObject` and `processDelayedInstance` (1 each at `e83d005d`; there is no `processDelayedSolid`).
  - Services: `CRenderer::retrieveVariable` (5), `locateFile`, `texturePath`,
    `findCoordinateSystem` (`environmentLoad` is Texturing's, not a Geometry use).
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
- **Measured by the architecture check** (spec 028, macOS): 52 edges allowlisted under DB-004
  (65 when spec 022 measured it).
  - 28 includes: `renderer.h` (17), `rendererContext.h` (9), `riInterface.h` (2).
  - 24 link references to `CRenderer` members (37 before spec 028).
  - Geometry's other cross-domain edges are registered elsewhere, so they are not counted
    here: diagnostics, RI tokens and `rendererc.h` were under DB-016 (41 edges, resolved by
    spec 023); symbols defined by
    `geometryDispatch.cpp`/`geometryDispatchStub.cpp` under DB-017; Shading and Hiders
    coupling under DB-006; the data viewer under DB-025.
- **History**: spec 021 (deferred CSG) resolves solids and procedural operands through a new
  Geometry-owned host interface rather than extending `processDelayed*`/`delayedMutex`; the
  existing uses remain.
  - Spec 028, 2026-10-29: the intersection-time motion and camera-rotation reads moved into the
    Geometry-owned `geometry/motion.h`, which reads the per-frame `FrameParams` installed by the
    renderer core (`state/frameParams.h`). 13 allowlist lines were deleted: the 6 link lines
    `dlobject.cpp` and `quadrics.cpp -> CRenderer::cameraHasRotation/relRotQ/toWorld`, and the 7
    `-> CRenderer::invShutterTime` lines of `quadrics.cpp`, `patches.cpp`, `polygons.cpp`,
    `curves.cpp`, `points.cpp`, `patchgrid.cpp` and `bsplinePatchgrid.cpp`. The Geometry
    `CRenderer::` metric fell from 78 to 48 references and 19 to 15 members.
  - Evidence for spec 031 or a follow-up (decision Q4): after spec 028 these files still include
    `renderer.h` or `rendererContext.h` but name no `CRenderer` or `CRendererContext` member
    (`grep -E '\bCRenderer(Context)?\b'` finds nothing): `curves.cpp`, `dlobject.cpp`,
    `implicitSurface.cpp`, `objectMisc.h`, `patches.cpp`, `patchgrid.cpp`, `points.cpp`,
    `polygons.cpp`, `quadrics.cpp`, `subdivision.cpp` and `subdivisionCreator.cpp`. The include
    lines stay valid allowlist lines; spec 028 removes no include.
  - Spec 038 (2026-11-04), no allowlist change: `libRIBVector` (the geometry-only library) compiles
    only the Core files pinned in `tests/architecture/ribvector-core-residual.txt`
    (`SceneInterface_RibVectorResidual`; the list may only shrink). Those that belong to this entry:
    `render/rendererDeclarations.cpp` (needed by `geometry/blobby.cpp`: `CRenderer::retrieveVariable`),
    `render/rendererFileSearch.cpp` (needed by `geometry/blobbyRepeller.cpp`: `CRenderer::locateFile`)
    and the two files that file needs, `render/rendererNetwork.cpp` and `render/rendererMutexes.cpp`,
    and `render/rendererStatics.cpp` (the `CRenderer` statics Geometry's `object.cpp` and
    `blobbyRepeller.cpp` read; also Hiders' and the data viewer's, below). Spec 038 also took
    `render/rendererClipping.cpp` and `render/resource.cpp` out of `libRIBVector`.
  - Spec 031, 2026-11-11: 52 allowlist lines to 3, all of them `blobbyRepeller.cpp`'s (its `renderer.h` include, `CRenderer::locateFile`
    and `CRenderer::texturePath`, spec 038's residual). The frame limits and constants (`numThreads`, `maxGridSize`, `maxEyeSplits`,
    `flags`, `raytracingFlags`, `projection`, the pixel and image-plane values, `clipMin`, `cocFactorPixels`, `invFocaldistance`) are a
    Geometry-owned value, `CGeometryFrame` (`geometry/geometryFrame.h`), that the renderer core builds from its own statics and installs
    once per frame after the hider contexts exist; the delayed-object and delayed-instance expansion and the coordinate-system lookup go
    through `CGeometryHost` (`expandDelayedObject`, `expandDelayedInstance`, `findCoordinateSystem`, implemented by `CRendererContext`); the
    variable lookup of `blobby.cpp` is `CDeclarations::find`; `instantiate()` reports through the Geometry-owned `IObjectSink` (header
    `geometry/objectSink.h`, implemented by `CRiInterface`), so `riInterface.h` and `rendererContext.h` are no longer included; the
    tessellation lock is Geometry's own (`surface.cpp`) and `CRenderer::tesselateMutex` is deleted; `render/geometryDispatch.cpp` is gone,
    its bodies live in their classes' files, and no Geometry file includes `renderer.h`, `rendererContext.h`, `riInterface.h`, `shading.h`,
    `shader.h` or `reyes.h` (`Geometry_NoHiderShadingNames`). The Geometry `CRenderer::` metric fell from 48 to 2 references and 15 to
    2 members. The entry stays Open for those three lines.

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
- **Tracking**: remediation spec 031 ([#37](https://github.com/juvenal/openrender/issues/37)); spec 028 ([#34](https://github.com/juvenal/openrender/issues/34)) did the raytrace side
- **Evidence**:
  - Depth of field has two models: Reyes's circle-of-confusion model (`CSampler::circleOfConfusion`,
    `sampler.cpp:23`, 5 call sites in `reyes.cpp`: `:1165`, `:1469-1470`, `:1888-1889`; the aperture
    tests are `reyes.cpp:172`, `:617`, `:1154`, `:1467`, `:1554`, `:1739`, `:1887`, and the aperture
    jitter slots `jdx`, `jdy` are `stochastic.h:82`) and the camera layer's thin lens
    (`CFrameCamera::generateRay`, `src/ri/state/camera.cpp:94`), which only the raytracer uses.
  - Reyes keeps its own motion and camera-rotation code, with its own SLERP of the camera
    rotation: `reyes.cpp:1427-1433`, `:1600-1604`, `:1637-1643`; `stochastic.cpp:166-169`, `:220`;
    `stochasticQuad.h:511-520`, `:590`, `:873-884`. It reads `CRenderer::cameraHasRotation` and
    `CRenderer::relRotQ` (DB-011), where the raytrace side reads `motion::cameraRotationAt` and
    `transformRay` (`geometry/motion.h`), pinned equal to Reyes's SLERP by
    `Motion_CameraSlerpEquivalence`.
- **Size**: not a dependency edge; tracked by review.
- **Direction**: one camera layer (`ICamera::generateRay`, `FrameParams`) and one Geometry-owned
  motion evaluator used by dicing and intersection alike. Spec 028 moved the raytrace side; the
  Reyes side is spec 031.
- **History**: spec 028, 2026-10-29: the raytrace side moved onto `ICamera`, `FrameParams` and
  `motion.h` (Geometry's intersection-time motion no longer reads `CRenderer::invShutterTime`);
  Reyes's side is spec 031.
- **History**: spec 031, 2026-11-11, camera-rotation half: Reyes and the stochastic rasterizer read the camera rotation, translation, flags and lens values
  from `FrameParams` (`FrameParams::cocFactorScreen` and `cocFactorSamples` are copies of the core's values, removed by the lens phase) and rotate
  through `motion::cameraRotationAt`, the one function the raytrace side uses; the inlined `slerpq` + `qtoR` pairs and their `identQ` constants
  are gone from `reyes.cpp`, `stochastic.cpp` and `stochasticQuad.h` (`Motion_CameraSlerpEquivalence`, `Motion_Replay_*` and
  `Motion_FrameParamsEqualCore` pass; no image changed). Reyes's depth-of-field model (the circle of confusion) is the lens half: the entry stays Open.

### DB-010 — The hiders' GI caches use Texturing

- **Rules**: the allowed-dependency table (Hiders may not depend on Texturing).
- **Tracking**: remediation spec 032 ([#38](https://github.com/juvenal/openrender/issues/38)); spec 027 ([#33](https://github.com/juvenal/openrender/issues/33)) removed the rest
- **Evidence**: the hiders' GI caches use Texturing: `irradiance.cpp`, `irradiance.h` and
  `irradianceDispatch.cpp` include `texture.h`, `texture3d.h` and `pointHierarchy.h`, and
  `irradiance.cpp` constructs and destroys `CTexture3d` (its constructor, destructor and type
  information).
- **Size**: 8 edges (5 include, 3 link).
- **Direction**: GI-cache storage moves to Texturing with sampling left in Hiders (032).
- **History**: spec 027 (2026-10-28), 35 → 8 edges: 25 allowlist lines deleted (Texturing's reads
  of the renderer core, Graphics State and Shading: the texture state, the file search, the option
  block and the shading-context reads moved into the texture system, the file resolver and
  `TextureOpt`) and 2 re-tagged to DB-021. Texturing now includes nothing from another domain
  except `texture3d.cpp`'s `renderer.h` and `displayChannel.h`.
- **Texture system notes** (spec 027, not registry entries):
  - The core passes `&CRenderer::shaderMutex` as the texture system's `loadMutex`. It is a plain,
    non-recursive mutex, so no code that holds it may call `CTextureSystem::handle()`; the services
    getters that hold it (`getCache`, `getTexture3d`, `getPhotonMap`) never reach a texture handle,
    and a new caller that does is a self-deadlock. A debug build asserts a re-entry of `handle()`.
  - A consumer of the texture system outside a renderer must initialize libRIBCommon's arena
    (`globalArena.init()`; `RiBegin` does it in a renderer): synthesizing an unbaked source decodes
    into it.

### DB-011 — Hiders read renderer-core state (`CRenderer`)

- **Rules**: the renderer-core rule (no domain may depend on the renderer core).
- **Tracking**: remediation specs 028 ([#34](https://github.com/juvenal/openrender/issues/34)), 029 ([#35](https://github.com/juvenal/openrender/issues/35)), 030 ([#36](https://github.com/juvenal/openrender/issues/36))
- **Evidence**:
  - 19 files under `src/ri/hiders/` make 556 `CRenderer::` references (the archcheck metric,
    `metrics-baseline.txt`; a raw grep counts 573): frame and
    sampling parameters (`clipMax`, `bucketWidth`, `pixelXsamples`, `xSampleOffset`, …),
    `flags`, `hiderFlags`, `raytracingFlags`.
  - 9 files include `src/ri/render/renderer.h`.
- **Size**: 164 edges per platform and 164 DB-011 lines after spec 029 (9 include, 155 link). After
  spec 028 it was 166 edges (9 include, 157 link) and 168 lines, because the thread-local
  `CRenderer::activeContext`, referenced through Clang's TLS wrapper (`_ZTW…`) on macOS and directly
  by GCC on Linux, had its 2 edges listed once per platform; before spec 028, 173 edges (9 include,
  164 link) and 175 lines. The previous header and Size text said
  182 lines and 180 edges; they were stale (the file held 175 lines at the start of spec 028).
  Linux counted 173 edges before spec 028 and 166 after it (`specs/028-camera-and-motion-layer/validation.md`
  §3 and §7), the same as macOS.
- **Direction**: inject immutable `FrameParams` and per-thread contexts; the mutable flags become
  capabilities on the injected interfaces. Each hider-side spec removes the statics it uses.
- **History**: spec 028, 2026-10-29: the raytracer takes its camera from `ICamera` and the
  per-frame `FrameParams` instead of the statics. 7 allowlist lines were deleted:
  `raytracer.cpp -> CRenderer::imagePlane`, `projection`, `focaldistance`, `invImagePlane`,
  `aperture`, `dxdPixel` and `dydPixel`. The Hiders `CRenderer::` metric fell from 567 to 558
  references and 84 to 82 members.
- **History**: spec 039, 2026-10-20: the hiders take every camera value from the sampler's
  camera slots through an immutable per-frame `CSamplingConfig`, so the Legacy camera path's
  reads went with it. Deleted allowlist lines: `raytracer.cpp` and `stochastic.cpp ->
  CRenderer::correlatedSampleTable` (the correlated table, deleted), `stochastic.cpp ->
  CRenderer::frame` (the seed of the deleted `apertureGenerator`), and `raytracer.cpp` and
  `stochastic.cpp -> CRenderer::jitter`. The Hiders `CRenderer::` metric fell from 586 to 567
  references and 87 to 84 members.
- **History**: spec 029, 2026-11-04: the hiders no longer write `CRenderer::activeContext` (`reyes.cpp`, `raytracer.cpp`; the renderer core sets it in
  `rendererDispatchThread` for every hider). 4 allowlist lines were deleted (the `_ZTW…` wrapper pair on macOS, the plain symbol pair on
  Linux): 164 DB-011 lines (168 before), 164 edges per platform (166 before). The Hiders `CRenderer::` metric fell from 558 to 556 references
  and 82 to 81 members.

- **History**: spec 038 (2026-11-04), no allowlist change: `render/rendererStatics.cpp`, the file that
  defines the `CRenderer` statics the hiders read, is one of the Core files `libRIBVector` still
  compiles (`tests/architecture/ribvector-core-residual.txt`), needed by `hiders/compositor.cpp`,
  `hiders/irradiance.cpp` and `hiders/photonMap.cpp` as well as by Geometry (DB-004) and the data
  viewer (DB-024). It leaves the list when the hiders stop reading the statics (specs 029 to 031).
- **History**: spec 031, 2026-11-11: 13 allowlist lines deleted, 164 to 151. `hiders/photonMap.cpp` reads the camera matrices from `FrameParams` (`CRenderer::toWorld`
  and `fromWorld`: 2 lines); Reyes and the stochastic rasterizer read the aperture, lens, camera-rotation and camera-translation values from
  `FrameParams` (`reyes.cpp -> CRenderer::aperture`, `cameraHasRotation`, `cocFactorSamples`, `cocFactorScreen`, `invFocaldistance`, `relRotQ`,
  `relTrans`; `stochastic.cpp -> CRenderer::cameraHasRotation`, `cameraRotationOnly`, `relRotQ`, `relTrans`: 11 lines). The Hiders `CRenderer::` metric fell
  from 556 to 499 references and 81 to 72 members (the Phase 6 photon-map change took 4 references and 2 members of it).

### DB-013 — No hider registry: `strcmp` chains in the renderer core

- **Rules**: 3; adding a hider must not require core edits.
- **Tracking**: remediation spec 030 ([#36](https://github.com/juvenal/openrender/issues/36))
- **Evidence**:
  - Two `strcmp(hider, …)` chains in `src/ri/render/renderer.cpp:778-830`, plus the
    `"stochastic"` alias in `rendererContext.cpp:1529`.
  - The built-in `CSphereLight`/`CQuadLight` fallback (`src/ri/hcshader.{h,cpp}`, 776 lines) is
    still compiled; they were friends of `CShadingContext` until spec 029 dropped the friendships.
- **Size**: not a dependency edge; tracked by review.
- **Direction**: a self-registering hider factory (name, aliases, capabilities); retire
  `hcshader` now that the RSL `spherelight`/`quadlight` ship.
- **History**: spec 025 (2026-10-27): an attribute block's active-light list holds non-owning
  `CShaderGroupRefTarget *` entries. The unchecked cast of an entry to a
  `CProgrammableShaderInstance *` (the built-in `CSphereLight`/`CQuadLight` are not programmable
  instances; the consumers read only `CShaderInstance` members) now lives in `lightInstanceOf()`
  (`src/libshader/shading/shaderRefs.h`), preserved as it was.

### DB-017 — Link-time `*Dispatch` / `*DispatchStub` substitution pairs

- **Rules**: 2 and 3 (dependencies go through interfaces, not through which file the linker
  receives).
- **Tracking**: remediation spec 031 ([#37](https://github.com/juvenal/openrender/issues/37))
- **Evidence**: four pairs, swapped by `src/ri/CMakeLists.txt` (`ribVector_sources`, lines 374-418)
  depending on the library being built: `render/geometryDispatch.cpp` / `vector/geometryDispatchStub.cpp`,
  `render/rendererDeclarationsDispatch.cpp` / `vector/rendererDeclarationsDispatchStub.cpp`,
  `geometry/surfaceCrossingDispatch.cpp` / `vector/surfaceCrossingDispatchStub.cpp` (spec 021),
  `hiders/irradianceDispatch.cpp` / `vector/irradianceDispatchStub.cpp`. Geometry methods
  (`CSurface::dice`, `CSurface::shade`, `CSurface::regionCoverage`, `CObject::dice`, the
  `CSurface` vtable and typeinfo) are defined in the renderer-core file. The `rendererDeclarations`
  pair now carries `CRenderer::findCoordinateSystem` and `CRenderer::notifyGlobalVariable` (spec 038
  moved the declaration table to Graphics State's `CDeclarations`; the rest of
  `render/rendererDeclarations.cpp` is not part of the pair).
- **Size**: 78 link edges (Geometry 70, Hiders 7, plus one `rendererDeclarations` pair edge). A
  wrong assumption only fails at run time.
- **Direction**: delete each pair once its consumer is interface-based (Geometry's `IDicer`/
  `IRayContext`, injected RNG and state notifications); `ribVector` must still link without the
  stubs. The "injected RNG" part is done for `CObject::cluster()` (spec 039, 2026-10-20): its draws
  come from a `CKeyedStream` keyed by the object's `stableId` and the iteration, not from the
  shading context's generator; the pair itself remains for the other reasons above.
- **History**: spec 038 (2026-11-04), no allowlist change: the four stub files are the four
  `vector/*DispatchStub.cpp` entries of `tests/architecture/ribvector-core-residual.txt`
  (`SceneInterface_RibVectorResidual`), each with the Geometry or Hiders file that needs it
  (`geometry/object.cpp` for the declarations stub, `hiders/irradiance.cpp` for the irradiance stub).
- **History**: spec 031, 2026-11-11: three of the four pairs are deleted. The Geometry bodies of `render/geometryDispatch.cpp` live in `object.cpp`, `csgSolid.cpp`,
  `surface.cpp` (which now defines `CSurface`'s vtable and typeinfo), `curves.cpp`, `points.cpp` and `delayed.cpp`; `rendererDeclarationsDispatch.cpp` is
  merged into `rendererDeclarations.cpp` (a Core file `libRIBVector` no longer compiles); `surfaceCrossingDispatch.{h,cpp}` is `surfaceCrossings.{h,cpp}`, compiled
  for every consumer; `vector/geometryDispatchStub.cpp`, `vector/rendererDeclarationsDispatchStub.cpp` and `vector/surfaceCrossingDispatchStub.cpp` are gone.
  All 77 DB-017 allowlist lines were deleted (52 of them re-keyed in place first, Phases 4 and 5, as the signatures moved to `IRayContext` and `IDicer`; none added).
  The entry stays Open for the irradiance pair (`hiders/irradianceDispatch.cpp` / `vector/irradianceDispatchStub.cpp`), which is spec 032's and has no allowlist line;
  `Geometry_RibVectorNoStubs` exempts that pair's two `nm -u` symbols until 032 lands.

### DB-018 — Message passing reads `CAttributes` directly

- **Rules**: 3 (Shading reaches other domains' data through services).
- **Tracking**: remediation specs 026 ([#32](https://github.com/juvenal/openrender/issues/32)), 033 ([#39](https://github.com/juvenal/openrender/issues/39)), 034 ([#40](https://github.com/juvenal/openrender/issues/40))
- **Evidence**: spec 026 (2026-11-01): the eight builtins `attribute()`, `option()`, `rendererinfo()`,
  `surface()`, `displacement()`, `atmosphere()`, `incident()` and `opposite()` ask the services
  (`getAttribute`/`getMessage`) in both backends, and none reads
  `currentShadingState->currentObject->attributes`; the reads that remain in `shading.cpp` are the
  object's own, listed in spec 026's research R5.
  - Still open: the JIT's message-passing entry points (`op_*_param`, `op_lightsource_f`, spec 033);
    `shadername(type)`, a direct read null-guarded since spec 026; group-layer messages and
    `setmessage` (spec 034); the `lightsource()` divergence between the interpreter and the JIT
    ([#115](https://github.com/juvenal/openrender/issues/115)); and the unknown destination size of a
    float-class query ([#121](https://github.com/juvenal/openrender/issues/121)).
- **Size**: not a dependency edge (Shading may depend on Graphics State); tracked by review.
- **Direction**: the ray type and hit mode in `ShaderGlobals` (spec 026 defined the contract and the
  situation table; spec 029 filled it, [#35](https://github.com/juvenal/openrender/issues/35): the hiders' producer sets `rayType`, `hitMode` and `transport` per batch from the
  situation table as corrected to the code (`classifyShading()`, `specs/029-hider-shading-separation/contracts/point-source-and-globals.md`) and `shade()` reads them);
  the JIT's message-passing entry points through the services (spec 033); `getmessage`/`setmessage`
  across shader-group layers (spec 034).

### DB-020 — Cross-domain `friend` declarations

- **Rules**: 3 (no back doors into another domain's internals).
- **Tracking**: remediation specs 030 ([#36](https://github.com/juvenal/openrender/issues/36)), 032 ([#38](https://github.com/juvenal/openrender/issues/38))
- **Evidence**:
  - `CShadingContext` no longer befriends `CPhotonHider`, `CSphereLight` and `CQuadLight` (spec 029, 2026-11-04; they
    were at `shading.h:1175,1177,1178` at the base commit); its only friend is Shading's own
    `CProgrammableShaderInstance`.
  - `CIrradianceCache` befriends `CRemoteICacheChannel` (`irradiance.h:137`); `CTexture3d` and
    `CPointCloud` befriend `CRemotePtCloudChannel` (`texture3d.h:100`, `pointCloud.h:149`).
  - `CBrickMap` befriends `CBrickMapGeometry` (`brickmap.h:254`), a class forward-declared at
    `brickmap.h:39` and defined nowhere.
- **Size**: not a dependency edge; tracked by review.
- **Direction**: narrow public accessors or interfaces, removed with the spec that touches each
  class; delete the dangling friend.

### DB-023 — Domains are not build targets

- **Rules**: 1 (each domain is a separately buildable unit).
- **Tracking**: spec 022 adds the check; every remediation spec 023–038 narrows include paths for
  its domain ([#29](https://github.com/juvenal/openrender/issues/29)–[#43](https://github.com/juvenal/openrender/issues/43), and [#26](https://github.com/juvenal/openrender/issues/26) for 037).
- **Evidence**:
  - `ribRender_obj` (`src/ri/CMakeLists.txt:178`) compiles sources from six domains plus the
    renderer core into one OBJECT library.
  - `libshader_shading`'s include path lists `src/ri/{core,parse,state,texture}` and `src/ri` itself (measured at the end of spec 029,
    2026-11-04; the Geometry directory was the last cross-domain one to leave it), so a Geometry
    or Hiders header cannot be included by Shading at compile time. Other domains' include paths are unchanged.
- **Size**: not a dependency edge; tracked by review and by the architecture check.
- **Direction**: per-domain include visibility first, then per-domain OBJECT targets as each layer
  lands.

### DB-024 — Tools reach into domain internals

- **Rules**: none broken (consumers may depend on anything). Registered as evidence for the
  public APIs the tools should use.
- **Tracking**: remediation specs 033 ([#39](https://github.com/juvenal/openrender/issues/39)), 037 ([#26](https://github.com/juvenal/openrender/issues/26)), 038 ([#43](https://github.com/juvenal/openrender/issues/43))
- **Evidence** (the check's `CONSUMER` section; spec 038 re-measured it on 2026-11-04: 71 edges, 32
  from `src/libshader/tests/*` and 39 from the tools; the registry's earlier 36 counted the tool edges
  without the two `ri.h` edges of `orender.cpp` and `otexmake.cpp`, which makes 37 now):
  - `sloinfo.cpp` and `oshader.cpp` include Shading internals (3 each);
  - `precomp.cpp` includes one Hiders header, `rasterFlags.h` (the `RASTER_*` constants, no other
    content; it used to include `reyes.h` and the renderer core's `renderer.h`);
  - `libribpreview` includes 15 Geometry headers, 4 Graphics State headers (`pl.h`, `attributes.h`,
    `options.h`, `xform.h`) and 5 Scene Interface headers (`riHooks.h`, `rib.h` twice, `ri.h`,
    `riInterface.h`); the earlier text said 3 and 4. It includes no renderer-core header;
  - `src/ri/dataviewer/dataLoad.cpp` reaches into Hiders (2) and Texturing (2) and includes 2 Scene
    Interface headers (`riInterface.h`, `riServices.h`), and `renderer.h` for the `CRenderer::fromWorld`
    seeding (DB-025).
- **Size**: 37 consumer-reach edges from the tools (39 listed with the two `ri.h` edges); reported,
  never failing, no allowlist lines.
- **Direction**: tools use public APIs: a shader-metadata API from Shading (033), one display ABI
  definition (037). Spec 038 did not add `RiMake*` for precomputation (the earlier Direction): it cut
  `precomp` off the hider class and the renderer core by moving the constants into `rasterFlags.h`.
  Remaining: `sloinfo` and `oshader` (specs 033 and 043), the data viewer's Hiders and Texturing reach
  (spec 032), and the preview's Geometry and Graphics State includes.
- **History**: spec 037 (2026-10-31) did the "one display ABI definition" item: `dsply.h` is the single
  definition of the display interface, and the version 1 types survive only in the renderer core's
  private `displayV1Abi.h`, which the version 1 adapter alone includes.
  - Spec 038 (2026-11-04): `precomp` includes `src/ri/hiders/rasterFlags.h` instead of `reyes.h` and
    `renderer.h`; the preview starts its session with `RiBeginBackEnd("preview")` (a
    `CRiStandardSession`) and the data viewer holds a `CRiStandardSession`, so neither calls
    `CRenderer::initDeclarations()`/`shutdownDeclarations()` and the preview includes neither `renderer.h`,
    `rendererContext.h` nor `riDiagnostics.h`. The data viewer still includes `renderer.h` (DB-025).

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
- **History**: spec 031, 2026-11-11: `geometry/surface.cpp -> dataviewer/debug.h` is deleted (12 to 11 edges): the include and the two dead `DEBUG_HITS` and
  `DEBUG_TESSELATIONS` blocks that used `CDebugView` are removed. The entry stays Open for spec 032's 11 lines and is moved by whichever of 031 and 032 merges second.

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
    the arena and stats injection was not part of spec 029 (the hider/shading separation) and is tracked
    separately ([#135](https://github.com/juvenal/openrender/issues/135)).

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

### DB-026 — Texturing uses the RGBE codec in Display Outputs

- **Rules**: the allowed-dependency table (Texturing depends on nothing).
- **Tracking**: [#33](https://github.com/juvenal/openrender/issues/33)
- **Evidence**: spec 018's RGBE decoder, `src/ri/texture/imageInputRgbe.cpp`, includes
  `src/display/rgbe/rgbe.h` and calls `RGBE_ReadHeader`/`RGBE_ReadPixels`, which are defined in
  the Display Outputs domain. Found by the check; not one of the audit's V1–V21.
- **Size**: 3 edges per platform (1 include, 2 link). `FILE*` mangles as `__sFILE` on macOS and
  `_IO_FILE` on glibc, so the 2 link edges are listed once per platform, tagged (5 allowlist lines).
- **Direction**: move the RGBE codec into infrastructure (`src/common`), shared by the display
  driver and the texture decoder.
- **Resolved** by spec 027 (`specs/027-texture-system/`), 2026-10-27:
  - 3 edges per platform to 0, with the 5 allowlist lines (and their section header) deleted and
    nothing added or swapped;
  - the codec moved with `git mv` to `src/common/rgbe.{h,cpp}` (body unchanged) and its six
    functions are declared `extern "C"`, so the names read the same on macOS and glibc; it is
    compiled into `libRIBCommon` (`openrendercommon_obj`), the one definer, and no longer into
    `ribRender`, `ribVector` or the `rgbe` display module, which link it through the shared
    `openrendercommon`; `dsply.h` is untouched;
  - `Common_StateOwnership` checks the six `RGBE_*` names have exactly one definer;
    `Display_RgbeBytes` (a characterization test, byte-identical to the starting tree's driver
    output) and `ImageInput_Rgbe` guard the behaviour.

### DB-003 — Plug-in ABI headers carry no interface version

- **Rule**: 4 (plug-in interfaces MUST carry a Semantic Versioning 2.0.0 interface version; the host
  MUST load only a MAJOR the host supports and reject any other).
- **Tracking**: [#26](https://github.com/juvenal/openrender/issues/26) (enhancement)
- **Evidence**: none of the published plug-in headers declares a version — `src/ri/render/shadeop.h`
  (Shader DSO Operands), `src/ri/geometry/dlo.h` and `src/ri/geometry/implicit.h` (Geometric
  Procedures), `src/display/dsply.h` (Display Outputs) — and the loaders therefore cannot check one.
- **Direction**: add a `MAJOR.MINOR.PATCH` interface version per header (macros plus an exported
  query symbol each plug-in provides), have the host validate it at load time, and document each
  header's version history. Existing plug-ins without the symbol need a defined transition rule
  (e.g. treated as `1.0.0` with a warning for one release).
- **Resolved** by spec 037 (`specs/037-plugin-abi-semver/`), 2026-10-31:
  - every plug-in header declares its interface version in macros and an exported version
    symbol, with a version-history table in its header comment: display 2.0.0 (`dsply.h`),
    shadeop 1.1.0 (`shadeop.h`), dlo 1.0.0 with dynamicload 1.0.0 (`dlo.h`), implicit 1.0.0
    (`implicit.h`); the encoding, the export macros and the status codes are the shared
    `orplugin.h`/`orplugin.hpp`;
  - one routine, `checkPluginVersion` (`src/common/pluginAbi.{h,cpp}`, `libRIBCommon`), makes the
    load decision for every family: a supported MAJOR loads, a newer MINOR loads with one warning,
    any other MAJOR is rejected; a module without the symbol loads as 1.0.0 with one warning and
    is rejected from openRender 1.2.0 (`OR_PLUGIN_UNVERSIONED_SUNSET`), `DynamicLoad` generators
    excepted;
  - the function-table size rule and the table header guard the display table;
  - `PluginAbi_VersionHistory`, `PluginAbi_SingleCheckRoutine`, `PluginAbi_Convention` and the
    per-family `*Abi_Versions` tests guard it; the numbers the guides quote are compared with the
    headers by `PluginAbi_DocPaths`.


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
- **Resolved** by spec 037 (`specs/037-plugin-abi-semver/`), 2026-10-31:
  - display 2.0.0: a function table, a per-display context and a `const` snapshot with copies of
    the renderer's values (no pointer to a renderer-core static), channel descriptors and a
    documented threading and lifetime contract, loaded by `CDisplayHost` in the renderer core with
    no file-static "current display"; MAJOR 1 drivers and `RiCustomDisplay` run through a confined
    adapter (`displayV1Adapter.{h,cpp}`) that gives them mutable copies and serializes their data
    calls;
  - shadeop 1.1.0: optional per-thread init and cleanup, a documented concurrency rule, a
    lexicographic scan order shared by the renderer and the compiler, and release at `RiEnd`
    before the module is unloaded (`libRIBShadeop`, `src/shadeop/`);
  - one Procedures module (`src/procedures/`) owns the `dlo`, `implicit` and `DynamicLoad`
    loaders, with a frame reference that keeps a generator's module loaded until that frame's
    geometry is destroyed; a bare module name is found on the procedural search path.


### DB-021 — Texturing reads the AOV/channel registry

- **Rules**: the allowed-dependency table (Texturing depends on nothing).
- **Tracking**: remediation spec 037 ([#26](https://github.com/juvenal/openrender/issues/26))
- **Evidence**: `src/ri/texture/texture3d.cpp` includes `src/ri/state/displayChannel.h` and
  `src/ri/render/renderer.h`, and builds its channel list by calling `CRenderer::retrieveDisplayChannel`,
  an out-of-line forwarder in the renderer core that, since spec 025 (2026-10-27), forwards to the
  Graphics State-owned `CChannelRegistry` (`src/ri/state/channelRegistry.{h,cpp}`). Texturing itself still
  reads the registry through the renderer core (the link edge) and still includes the channel type.
- **Size**: 3 edges (2 include, 1 link).
- **Direction**: the registry is owned by Graphics State (done in 025) and passed to its consumers
  as channel descriptors (037), so that Texturing no longer names `displayChannel.h` or calls into
  the renderer core for it.
- **History**: spec 025 moved the registry out of the renderer core's statics into Graphics State.
  `texture3d.cpp` was deliberately left untouched, and the forwarder was kept out of line so that
  `texture3d.o` references no Graphics State symbol: that edge and the link line are
  unchanged. Spec 027 (2026-10-28) re-tagged the `renderer.h` include and the
  `retrieveDisplayChannel` link of `texture3d.cpp` here from DB-010 (they are the same dependency);
  `texture3d.cpp` itself was not edited.
- **Resolved** by spec 037 (`specs/037-plugin-abi-semver/`), 2026-10-31: Texturing reaches the channel
  registry through its own interface (`src/ri/texture/texture3dChannels.{h,cpp}`, a
  `CTexture3dChannelSource` the core installs at `RiBegin` and removes at `RiEnd`, backed by the
  registry through `CRendererChannelSource`). `texture3d.cpp` includes neither `renderer.h` nor
  `displayChannel.h`, and `CRenderer::retrieveDisplayChannel` is deleted: 3 edges to 0, with the
  3 allowlist lines deleted and nothing added. `Texture3dChannels_Session`, `_NoSession`,
  `_Unknown` and `_FillAliased` guard it.

### DB-001 — Shading resolves renderer-core symbols at load time

- **Rules**: 2 (acyclic dependencies; no upward dependency via load-time-bound symbols), and the
  renderer-core rule (no domain may depend on the renderer core).
- **Tracking**: [#24](https://github.com/juvenal/openrender/issues/24) (enhancement / design review)
- **Evidence**: `libshader_shading` (Shading) links with `-undefined dynamic_lookup` (macOS) /
  `--allow-shlib-undefined` (Linux) in `src/libshader/shading/CMakeLists.txt`, and binds
  4 distinct `libRIBRender` symbols flat-namespace (`nm -m`, "dynamically looked up"), all in Hiders:
  `CGatherBundle`, the `CTraceBundle`/`CTransmissionBundle` vtables, `CPhotonMap::lookup`.
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
  - Spec 027, 2026-10-28: the 3D channel operations of `texture3d()`/`bake3d()` and the irradiance
    cache's `resolve` go through `CRendererServices` (`texture3dResolve`/`Lookup`/`Store`), so
    `CTexture3d::resolve` is no longer imported; 7 → 6 imports, edge count unchanged. The record is
    `specs/027-texture-system/tools/libshader-dynlookup-{before,after}-macos.txt`.
  - Spec 026, 2026-11-01: Shading's trace calls go through the Shading-owned `ITracer`
    (`tracer.h`), reached through `CRendererServices::tracer()` and answered by the hiders' adapter
    `CHiderTracer`; `trace`/`traceEx` are no longer imported (6 → 4 imports, 8 → 5 edges). The record is
    `specs/026-shading-contract-v2/tools/libshader-imports-{base,after}-macos.txt`.
- **Measured by the architecture check** (macOS, before spec 029): 5 edges allowlisted under DB-001 (8 before
  spec 026, 18 before spec 024, 30 before spec 023).
  - 5 link references from Shading sources: `CGatherBundle`, the `CTraceBundle`/`CTransmissionBundle`
    vtables and the two `CPhotonMap::lookup` references.
  - No includes: the 4 of `src/ri/render/renderer.h` are gone.
  - Shading's source-level coupling is registered separately: hider headers under DB-005,
    Geometry headers under DB-006 (the `dso.h` edges once registered under DB-019 are gone since spec 037).
- **Consequences before spec 029**: `libshader_shading` could not load without `libRIBRender` in the process; a
  static engine link had to whole-archive `libRIBRender.a` (the `openRender::RIBRender_static`
  package target applied `-force_load` for this).
- **Direction**: generic infrastructure is in `libRIBCommon` (spec 023), the renderer-core lookups go
  through `CRendererServices` (spec 024), the trace calls through `ITracer` (spec 026), and the bundles
  and the photon map were inverted by spec 029. Options were analysed in #24.
- **Resolved** by spec 029 (`specs/029-hider-shading-separation/`), 2026-11-04:
  - 4 imports to 0, and 5 allowlist lines deleted with nothing added: `CGatherBundle`, `CTraceBundle` and
    `CTransmissionBundle` moved into Shading (`src/libshader/shading/bundles.{h,cpp}`), and the photon map
    is reached through the Shading-owned `IPhotonLookup` (`photonLookup.h`), which `CPhotonMap` implements.
    `kRendererServicesVersion` is 2.1.0 (the photon-map accessor's type changed; `IPhotonLookup`,
    `IIlluminationHook`, `IPointSource`, `IShadingProducer` and `BatchShape::PointSet` are new).
  - `libshader_shading` links with undefined symbols as an error (`-undefined error` on macOS, which is
    ld64's default, and `--no-undefined` on Linux); `-undefined dynamic_lookup`, the INTERFACE
    `--allow-shlib-undefined`, the package's `-force_load`/`--whole-archive`/`--export-dynamic` block, the
    four `--no-as-needed` test options and the load stub of the standalone contract programs are gone. A
    program linked plainly against `ribRender_static` renders the #24 scene (the target carries one INTERFACE
    `-u ribrender_context_anchor` option so the render back-end's registrar is kept, P-Q17 option (a); `Link_StaticConsumer`;
    `Link_PackageStaticConsumer` through the installed package, opt-in with `-DOPENRENDER_LINK_PACKAGE_TEST=ON`).
    `LibShader_McompConsistency` (#44) passes on Linux.
  - The records are `specs/029-hider-shading-separation/tools/libshader-imports-{base,after-phase3,after}-{macos,linux}.txt`
    (the import lists before and after; the final lists are empty on macOS and 461 undefined dynamic
    symbols, none defined by `libRIBRender`, on Linux). `Link_ShadingSelfContained` guards it.

### DB-005 — Hiders and Shading share one class (`CShadingContext`)

- **Rules**: 2 (acyclic dependencies); the allowed-dependency table (Shading may not depend on
  Hiders).
- **Tracking**: remediation spec 029 ([#35](https://github.com/juvenal/openrender/issues/35))
- **Evidence** (at the base commit `01501f10`, before spec 029):
  - `CReyes` (`reyes.h:106`), `CRaytracer` (`raytracer.h:145`) and `CPhotonHider`
    (`photon.h:41`) inherited `CShadingContext`, which is declared in Shading
    (`src/libshader/shading/shading.h`), and `CPhotonHider`, `CSphereLight` and `CQuadLight` were its
    friends (`shading.h:1175,1177,1178`).
  - Its trace bodies were defined in Hiders: `CShadingContext::trace`, `traceEx` and `trace(CRay *)` in
    `src/ri/hiders/trace.cpp:47,253,303`.
  - Shading includes hider headers: `bundles.h` (4), `irradiance.h` (2), `photonMap.h` (2).
- **Size**: 8 include edges. The matching link references (trace, bundles, photon map) are
  counted under DB-001.
- **Direction**: composition. Shading's `ShadingContext` keeps state and execution with no hider
  knowledge, and is owned by Hiders' `CHider`; the hiders implement a Shading-owned `ITracer`.
  Strangler steps with byte-identical renders at each step (done by spec 029).
- **History**: the per-context RNG state is gone, the RNG part of V1 (spec 039, 2026-10-20):
  `CShadingContext` no longer holds a generator (the Mersenne Twister, `urand()`/`irand()` and the
  `random2d/3d/4d` CSobol members were deleted); every draw comes from the sampler's sub-stream
  of the sample's identity, so the shared class no longer carries random state between the
  hider and the shader it runs. The class sharing itself (this entry) is unchanged.
- **Resolved** by spec 029 (`specs/029-hider-shading-separation/`), 2026-11-04:
  - 8 include edges to 0, and 8 allowlist lines deleted with nothing added. `CShadingContext` is a `final`,
    virtual-free class; `CHider` (`src/ri/hiders/hider.{h,cpp}`) owns one and is its `ITracer` and its
    `IShadingProducer`; `CReyes` (with `CStochastic` and `CZbuffer`), `CRaytracer` and `CPhotonHider` derive
    from `CHider`, the last also from `IIlluminationHook`. The trace bodies are `CHider` members in `trace.cpp`
    (`traceGroups`, `traceRay`, `traceBundle`); the depth rule is applied only by
    `CShadingContext::enterRayLevel()`; `CHiderTracer` and `CRenderer::frameTracer` are gone. The four
    illumination-hook virtuals became `IIlluminationHook` (`illuminationHook.h`), and the three friends are gone.
  - Shading includes no hider header (`bundles.h` is Shading's own; `irradiance.h` and `photonMap.h` are not
    included) and, since spec 029's Phase 7, no Geometry header. The records: no image changed on either
    platform, the trace replay is unchanged. `ShadingContext_Standalone{Interp,Jit}`,
    `ShadingContract_IlluminationHookInterface{Interp,Jit}`, `HiderTracer_PerContext`,
    `ShadingContract_RayLevel` and `ShadingScope_Greps` guard it.

### DB-015 — Scene Interface depends on the renderer core

- **Rules**: the renderer-core rule.
- **Tracking**: remediation spec 038 ([#43](https://github.com/juvenal/openrender/issues/43))
- **Evidence** (as registered; 10 allowlist lines, 6 include and 4 link, 11 before spec 023):
  - The generated RIB and `ifexpr` parsers include `renderer.h`/`rendererContext.h`, and the
    RIB parser calls `CRenderer::locateFile` and `retrieveVariable`. (The `variable` parser moved
    to `libRIBCommon` in spec 023 and no longer includes `renderer.h`.)
  - `ri.cpp` calls `CRenderer::initDeclarations`/`shutdownDeclarations`.
  - `ri.cpp` and `ribOut.cpp` include `src/ri/render/riHooks.h`.
  - `CRiInterface`'s constructor installs itself as the global `renderMan`
    (`src/ri/parse/riInterface.cpp:37`).
- **Size**: 0 edges, 0 allowlist lines (10 edges before spec 038).
- **Resolved** by spec 038 (`specs/038-scene-interface-untangle/`), 2026-11-04: the Scene Interface
  (`src/ri/parse/`, `src/ri/output/`, the generated parsers) includes no renderer-core header and
  references no `CRenderer`/`CRendererContext` symbol. The 10 allowlist lines were deleted and none
  was added; the `SceneInterface` metric went from 6 references and 6 members to 0 and 0 (TOTAL 612/94
  to 606/90). What replaced each edge:
  - `ifexpr_generated.cpp -> renderer.h`, `rendererContext.h` (2 include): the condition evaluator is
    the Scene Interface function `riEvaluateCondition(CRiInterface *, const char *)`
    (`parse/riCondition.h`, `ifexpr.y`), which asks the active context for attributes and options;
    `CRendererContext::ifParse` is gone.
  - `rib_generated.cpp -> renderer.h`, `rendererContext.h` (2 include) and `-> CRenderer::locateFile`,
    `CRenderer::retrieveVariable` (2 link): the grammar calls `riRetrieveVariable` and `riLocateFile`
    (`parse/riServices.{h,cpp}`), which go through two services the Scene Interface owns,
    `CRiDeclarationService` and `CRiFileSearchService`. The renderer core implements and installs
    them (`CCoreDeclarationService`/`CCoreFileSearchService`, `render/rendererDeclarations.cpp`); a
    program without a renderer (the RIB writer, the preview, the data viewer) installs the standard
    pair with `CRiStandardSession`. The declaration table itself is Graphics State's `CDeclarations`
    (`state/declarations.{h,cpp}`, reference counted, the same declared set and global numbering as
    before).
  - `ri.cpp -> CRenderer::initDeclarations`, `shutdownDeclarations` (2 link): the RIB-writer start-up
    holds a `CRiStandardSession` instead; the renderer's `initDeclarations` acquires the same table.
  - `ri.cpp`, `ribOut.cpp -> render/riHooks.h` (2 include): `riHooks.h` moved to `src/ri/parse/`.
  - The constructor's side effect (not an allowlist line): `CRiInterface`'s constructor assigns
    nothing; `riInstallContext`/`riUninstallContext`/`riPreviousContext` install a context
    explicitly, archive nesting uses `riPreviousContext` (the `savedRenderMan` global is gone), and
    `SceneInterface_ActiveContextAssignments` fails on an assignment of `renderMan` outside
    `src/ri/parse/`.
  - Behaviour change, accepted: reports raised while `CRendererContext` constructs reach the sink
    installed before `RiBegin` and no longer set `RiLastError` (NEWS).
  `SceneInterface_RibOutNoCore`, `SceneInterface_NoCoreInclude`, `SceneInterface_HooksAndRecords`,
  `SceneInterface_ActiveContextAssignments`, `SceneInterface_ContextInstall` and `_ContextInstallRules`
  guard it, and `libRIBOut` compiles no Core file.

### DB-019 — The shader compiler depends on Scene Interface and renderer-core headers

- **Rules**: the allowed-dependency table (Shading may not depend on Scene Interface or the
  renderer core).
- **Tracking**: remediation specs 038 ([#43](https://github.com/juvenal/openrender/issues/43)), 037 ([#26](https://github.com/juvenal/openrender/issues/26))
- **Evidence**: spec 037 (2026-10-31) removed the dependency edges: the compiler includes no Scene
  Interface or renderer-core header and no longer `dlopen`s shadeop DSOs itself; it reads
  shadeop declarations through the shadeop module (`src/shadeop/shadeopModule.h`, Shading to
  Shader DSO Operands, allowed). What remained then was not an edge but the reason the entry stayed
  open (both items are done, see Resolved below):
  - the JIT had no DSO shadeop call path (the two DSO dispatcher rows of `scriptFunctions.h`,
    named `XXX`, are resolved at run time and have no JIT emission), so a `.slo` shader could not
    call a DSO operand and needed the interpreter format;
  - the compiler still took its builtin and opcode tables from the shading engine's headers
    (`scriptFunctions.h`, `scriptOpcodes.h`) instead of owning them, so it was not standalone.
- **Size**: 0 include edges and 0 allowlist lines (3 include edges before spec 037; 4 before spec 023,
  which moved `rendererc.h` into `src/common` and so removed `llvmEmitter.cpp`'s include of it).
- **Direction**: a standalone compiler with its own builtin and opcode tables (done in spec 038;
  spec 043 keeps the public library and C API, decision D25), reading shadeop prototypes only through
  the versioned `shadeop.h` (done in spec 037); a JIT DSO call path (done in spec 038).
- **History**: spec 037 deleted the 3 `dso.h` include lines (the compiler's `dso.y` copy and its
  `dso_generated.cpp`, the unused include in `shading/rslo.y`, and `rslo.cpp`'s include) by
  moving the declaration parser and the scan into `libRIBShadeop`.
- **Resolved** by spec 038 (`specs/038-scene-interface-untangle/`), 2026-11-04: a `.slo` shader calls DSO
  operands (`op_dso_call`; the `openrender.shader.shadeops` slots, which the loader binds on every load;
  format 2.1.0 only for a module with such a call, a missing operand falls back to the `.rslo`); the
  compiler owns its builtin and opcode tables (`compiler/builtinTables.cpp`, generated by
  `test_libshader_compiler_table_drift --emit`, guarded by `LibShader_CompilerTableDrift`) and its include
  path holds no Scene Interface, renderer or shading-engine directory (`Compiler_IncludeBoundary`). No
  allowlist line was involved (0 before, 0 after). `ShadeopAbi_Jit_*`, `LibShader_GateHardening` and
  `LibShader_OpcodeCoverage` guard the DSO path.

### DB-006 — Geometry is coupled to Shading and to the hiders

- **Rules**: 2; the allowed-dependency table (Geometry may depend only on Graphics State and
  Geometric Procedures; Shading may not depend on Geometry).
- **Tracking**: remediation specs 026 ([#32](https://github.com/juvenal/openrender/issues/32)), 029 ([#35](https://github.com/juvenal/openrender/issues/35)), 031 ([#37](https://github.com/juvenal/openrender/issues/37))
- **Evidence**:
  - Geometry includes Shading headers 15 times (13 through the `src/ri/core/shading.h` forwarding
    shim) and calls `CShadingContext::shade`, `saveState` and `restoreState`.
  - Geometry includes `src/ri/hiders/reyes.h` from 9 files and calls `CReyes::drawObject` and
    `CReyes::drawGrid` (`surface.cpp`).
  - Shading included `object.h` (one file) until spec 029, closing a Geometry ↔ Shading cycle; it no longer does.
- **Size**: 30 edges (23 include, 7 link) after spec 029 (31 before); 0 after spec 031.
- **History**: Spec 026, 2026-11-01: `execute.cpp` and `executeMisc.cpp` no longer include `surface.h` (the
  executor's attribute reads go through `CShadingContext::objectAttributes()`); `shading.cpp -> object.h`
  stays for spec 029. The `ShaderGlobals` contract exists (`shaderGlobals.h`); nothing fills it yet.
- **History**: Spec 029, 2026-11-04: `shading.cpp` includes no Geometry header (`shading.cpp -> object.h` deleted: 31 → 30 edges) and the Geometry
  directory is off `libshader_shading`'s include path. `shade()` takes a `ShaderGlobals` and an `IPointSource` that the hiders implement over
  `CSurface` (`CSurfacePointSource`); `CShadingContext::shade(CSurface *, ...)`, the symbol Geometry's lines bind to, is an opaque forwarding
  entry to the owning hider's `IShadingProducer`. Geometry's other lines are unchanged and it gained no include of a Shading or Hiders header;
  its call sites reach the context through `CReyes::shadingContext()`.
- **Direction**: an OSL-style `ShaderGlobals` filled by Geometry for Shading (026, 029);
  Geometry's dice and intersect take Geometry-owned `IDicer`/`IRayContext` interfaces instead of
  `CReyes*`/`CShadingContext*` (031).
- **History**: spec 031, 2026-11-11: `IRayContext` (`geometry/rayContext.h`) and `IDicer` (`geometry/dicer.h`), implemented by the hiders, replace the `CShadingContext *` and
  `CReyes *` parameters of every Geometry virtual; the forwarding entries `CShadingContext::shade(CSurface *, ...)`, `displace`, `displaceEstimate`, `setProducer` and
  `IShadingProducer` are retired (`kRendererServicesVersion` 2.2.0); `EShadingDim` is Infra (`common/shadingDim.h`).
- **Resolved** by spec 031 (`specs/031-geometry-hider-agnostic/`), 2026-11-11:
  - 23 include edges and 7 link edges to 0, with all 30 allowlist lines deleted and nothing added: Geometry includes no Shading or Reyes header
    (`Geometry_NoHiderShadingNames` scans the directory, comments included) and `libRIBVector` compiles the real Geometry with no stub
    (`Geometry_RibVectorNoStubs`; its `nm -u` check names no `CShadingContext`, `CReyes`, `CHider` or `IShadingProducer` symbol of Geometry's).
  - The Geometry contract programs `Geometry_DiceThroughDicer`, `Geometry_IntersectThroughRayContext`, `Geometry_ExpansionThroughHost`, `Geometry_ObjectSink` and
    `Geometry_FrameDefault` build and run with no hider and no Shading. No image changed on either platform (the manifests, the `MaxBlockAvgDiff` list and the
    determinism sweep are equal to the baselines).
