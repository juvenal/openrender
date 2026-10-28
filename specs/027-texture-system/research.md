# Research: Texture System (spec 027)

Phase 0 of the plan. Every finding below was read from the tree at `bc261b1` (branch `027-texture-system`,
spec commit `0f76934`) or measured on the fresh macOS build `build-027` (2026-10-27). Line numbers are
`bc261b1`'s. Where a finding contradicts the spec, the item says so and the plan carries it as a question.

## R1. Starting measurements (fresh macOS build)

`build-027`: configured from scratch in the worktree (vcpkg, `CMAKE_BUILD_TYPE=Release`), built at `-j4`
through `cpujob`, clean under `-Werror`. Raw outputs are in `tools/`.

| Measurement | Value | Same as the spec's? |
|---|---|---|
| archcheck | `PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; `objects=256 sources=182 edges=2363 (include=1466 link=897)` | yes |
| `ALLOWED` | `DB-010 x 35`, `DB-021 x 1`, `DB-025 x 12`, `DB-026 x 3` (the other IDs unchanged) | yes |
| Texturing metric | refs 70, distinct 16 (baseline 70/16); TOTAL 721/110 | yes |
| The 16 members | listed by `tools/core_usage.py` (`tools/core-usage-texturing-bc261b1.txt`), which reproduces archcheck's count: `textureRefNumber` 11, `textureUsedBlocks` 10, `locateFile` 9, `numThreads` 9, `textureUsedMemory` 8, `textureMaxMemory` 6, `textureMutex` 5, `synthesizeMutex` 2, `textureCacheEnabled` 2, `textureCacheDirectory` 2, and 1 each for `environmentLoad`, `initTextures`, `shutdownTextures`, `textureLoad`, `texturePath`, `retrieveDisplayChannel`. By file: `texture.cpp` 62, `texmake.cpp` 6, `brickmap.cpp` 1, `texture3d.cpp` 1 | yes (70/16) |
| Shading flat-namespace imports | 7, the same set as `tools/libshader-dynlookup-before-macos.txt` (`tools/libshader-dynlookup-bc261b1-macos.txt`) | yes |
| `texture_tile` | 9/9 pass (`-j2`) | — |
| `image_input` | 6/6 pass | — |
| Texture-related visual, parity and shading-parity tests (`Parity_{texture,unbaked}-*`, `Parity_env`, `Parity_shadow`, `Visual_sphere-{occlusion,texture3d-bake3d,textureinfo}-reyes{,-slo}`, `ImageInput_Rgbe`, `test_data_pointcloud`, the occlusion `ShadingParity_JitPL*`, `Convergence_occlusion-raytrace`) | 40/40 pass | — |

archcheck reports per-class totals only; `tools/core_usage.py` blanks comments and literals exactly as
`tests/architecture/archcheck/metrics.cpp` does and prints the members behind the totals, so the plan can name
them (spec: "the plan lists the 16 exactly from archcheck's own output").

Not measured at the plan stage, left to the first measurement task (maintainer decision 4): the full suite
pass list, the image manifest and its run-to-run noise, the JIT texture probes (R16), the timing for SC-010
(R19), and the whole Linux run (R20).

## R2. What a lookup reads from the shading context today

Every lookup in `src/ri/texture/texture.cpp` and `pointHierarchy.cpp` that takes `CShadingContext *context`
reads only these fields:

| Read | Where | Used for |
|---|---|---|
| `context->thread` | `textureMemFlush`, `textureAllocateBlock`, `textureLoadBlock`, `CBasicTexture`/`CTiledTexture` lookups, `CDeepShadow` (`texture.cpp:151,223,845,881,1107,1230,1893,1965,2020`) | per-thread block data, reference number, memory counters |
| `context->threadMemory` | `textureMemFlush` (`texture.cpp:149,164,203`) | scratch list of active blocks during eviction |
| `scratch.textureParams.fill` | `CMadeTexture::lookup`, `CRegularTexture::lookup`, the dummies (`1289,1504,2374,2384,2394`) | out-of-range and missing-file value |
| `scratch.textureParams.{filter,blur,samples}` | `CMadeTexture::lookup4`, `CRegularTexture::lookup4` (`1346-1414`, `1554-1581`) | filter weight, blur, number of filter samples |
| `scratch.textureParams.{filter,blur}`, `scratch.traceParams.{samples,bias}` | `CShadow` (`1665-1700`), `CDeepShadow` (`1838-1882`) | shadow filter; **the sample count and bias come from the trace block** |
| `scratch.textureParams.{filter,blur}`, `scratch.traceParams.samples` | `CCubicEnvironment` (`2091-2146`) | environment filter; **sample count from the trace block** |
| `context->drawNext2` / `drawNext4` | `1403,1563,1578,1673,1853,2141` | stratified filter samples (spec 039) |
| `scratch.occlusionParams.maxSolidAngle` | `CPointHierarchy::lookup` (`pointHierarchy.cpp:474-475`) | point-based occlusion split test |

`CSphericalEnvironment` and `CCylindericalEnvironment` call `side->lookup4(…, context)`, so an environment
lookup on them reads the 2D block (`textureParams.samples`, not `traceParams.samples`). `CShadow` calls
`side->lookupz(…, context)`. The widths (`width`, `swidth`, `twidth`) are never read by a lookup: the caller
uses them to build the footprint (`shaderFunctions.h:1590-1591,1843-1844`).

**Consequence for the option block**: one `samples` field cannot reproduce today's reads. `TextureOpt` carries
both counts and the bias: `samples` (2D filter, from `textureParams.samples`), `traceSamples` and `bias` (from
`traceParams`, read by shadow, deep shadow and cubic environment). The call site copies both from the scratch,
so every lookup reads exactly the value it reads today. The irradiance hider's trick of setting
`traceParams.samples = 1` around its environment lookup (`irradianceDispatch.cpp:403-406`) keeps working
unchanged, because the option block is built after it.

## R3. `TextureOpt` and the sample stream (FR-003, FR-005)

**Decision**: a plain struct with no constructor, built at the call site after `plReady()` (per point, because
the parameters may be varying), passed to every lookup **by non-const reference**. It holds a
`TextureSampleStream` by value: `{openrender::sampling::Backend backend; SubstreamKey key; uint32_t dim;}` with
inline `next()`, `next2()`, `next4()` that call `openrender::sampling::get1D(backend, key, dim++, 0)`, which is
`inline` in `src/common/sampling.h:337` and exactly what `CShadingContext::keyedNext()` does
(`shading.h:891`). After the lookup the caller writes `stream.dim` back to `drawCursorDim`.

- Nested lookups share the stream: `CSphericalEnvironment`/`CCylindericalEnvironment` pass the same
  `TextureOpt&` to `side->lookup4`, `CShadow` to `side->lookupz`, so the dimension sequence is the one
  today's single `drawCursorDim` produces.
- Dimensions consumed per lookup (from the code, used by T-K1, data-model.md §5): `CMadeTexture::lookup4`
  2 per sample; `CRegularTexture::lookup4` 2 per sample, plus 2 more per sample when `blur > 0`; `CShadow` and
  `CDeepShadow` 4 per trace sample; `CCubicEnvironment` 2 per trace sample; spherical and cylindrical as their
  side texture's `lookup4`; point lookups, `lookupz`, the dummies 0.
- Where the advance is observable today: `bindDraws()` resets `drawCursorDim` to 0 at every bound point, but
  only when the call site has draw counters (`shading.cpp:737-742`). An unkeyed call (`counts == nullptr`) and
  the hiders' cursor (`irradianceDispatch.cpp` draws through `drawNext()` around its environment lookup)
  continue from where the last draw stopped. Writing the cursor back keeps those sequences identical.
- **Alternatives rejected**: (a) `TextureOpt` by const value with the stream returned separately — two
  channels for one state, easy to forget the write-back at one of the ~15 call sites; (b) a callback
  `float (*draw)(void*)` into the shading context — Texturing would hold a pointer into Shading's object and
  could not be tested without it; (c) Texturing drawing from its own per-thread stream — changes every
  stochastic texture render (SC-006).

## R4. Per-thread data (FR-004)

- **Range**: contexts are created for `i` in `[0, CRenderer::numThreads)` (`renderer.cpp:780-807`), after the
  network client forces `numThreads = 1` (`renderer.cpp:732-739`) and after `initTextures` reads it
  (`renderer.cpp:750`). The imager executor runs in `CRenderer::activeContext` or `contexts[0]`
  (`imager.cpp:127-134`), i.e. inside the pool. `rslBuiltins.cpp` uses `libshader::activeContext()`, a pool
  context. No context outside `[0, numThreads)` exists, so the system allocates exactly `numThreads` blocks,
  as `initTextures` does today.
- **Content** (`TexturePerthread`, opaque to every caller): the thread index; the reference counter (today
  `int *CRenderer::textureRefNumber[thread]`, one separately allocated `int` per thread, kept that way); used
  and maximum memory (today
  `textureUsedMemory[thread]`/`textureMaxMemory[thread]`, `maxMemory = ceil(total / numThreads)`); and a
  scratch arena (`CMemPage *`, `memoryInit`/`memoryTini` from libRIBCommon) for `textureMemFlush`'s
  active-block list, which today borrows the shading context's `threadMemory`.
- The blocks live in an array owned by the system, because `textureUnregisterBlock` walks every thread's
  counters (`texture.cpp:936-938`). `perthread(t)` returns `&blocks[t]`; it is a virtual call on the system,
  one per builtin call, not per point.
- **Alternative rejected**: keep borrowing the context's `threadMemory` through a pointer in the block. Texturing
  would hold a reference into a shading object, and the Texturing-only test (R15) could not run without one.
  The own arena costs one memory page per thread; it does not affect output.

## R5. Handles and the texture-family cache (FR-002, FR-014, Edge Cases)

- Today one `CTrie<CFileResource*> *frameFiles` holds every kind (`renderer.h:393`); the getters do
  find → load → insert with no lock of their own (`rendererFiles.cpp:436-490, 638-660`).
- **Decision**: the system owns a second `CTrie<CFileResource*>` for the texture family (2D textures,
  environments, shadow and deep-shadow maps, texture-info results). `frameFiles` keeps photon maps, irradiance
  caches and 3D textures (spec 032), with `framePhotonMaps` and #64's release loop in `endFrame()` untouched
  (`renderer.cpp:852-867`).
- **Keys stay what they are today**: one entry per name. `textureHandle(name)` reproduces `getTexture`
  (load as a 2D texture, else `CDummyTexture` plus `error(CODE_NOFILE, "Failed open texture \"%s\"\n")` unless
  the name is empty); `environmentHandle(name)` reproduces `getEnvironment` (else `CDummyEnvironment` plus
  `"Failed open environment \"%s\"\n"`); `infoHandle(name)` reproduces `getTextureInfo` (environment first, then
  texture; on a miss it returns `nullptr` and **inserts nothing**, so every later `textureinfo()` on a missing
  file probes again, as today — US2 AS3). A handle is the cached `CFileResource*` itself behind the opaque type.
- **Collision analysis** (spec Edge Cases):
  - *Across families* (a texture name also used for a point cloud, photon map or irradiance cache): today the
    second getter returns the first object cast to the wrong class (undefined behaviour). After the split each
    family loads its own object. This is the only behaviour change, and only for such a name.
  - *Within the family* (one name used with `texture()` and `environment()`): unchanged. The first kind loaded
    owns the name; a second kind gets the same object as today, including today's wrong-class cast. Keying by
    (name, kind) would fix it but loads twice in the `textureinfo()`-then-`texture()` case and changes the
    within-family behaviour; it is left as it is and recorded as a latent defect the maintainer may want to
    file (the plan writes no GitHub issue).
- Handles never outlive the system: the interpreter caches them in `CTextureLookup::map`,
  `CEnvironmentLookup::map`, `CMapInfoLookup::map` and `COcclusionLookup::environment`, which live in the
  context's `plHash`/`jitPlHash` (`shading.h`), and the contexts are deleted at the top of `endFrame()`
  (`renderer.cpp:827-832`), before the system.

## R6. GitHub #20: the find-load-insert race (FR-016)

- **Finding: on `bc261b1` the race #20 describes is not reachable through any production path.** Every call to
  `CRenderer::getTexture`/`getEnvironment`/`getTextureInfo` goes through `CRendererServicesImpl`, which holds
  `CRenderer::shaderMutex` around it (`rendererServicesImpl.h:62-97`); grep finds no other caller. The
  unlocked sequence #20 names is real in `CRenderer::getTexture` itself, which documents no locking
  requirement, but its only caller serializes it. The same wrapper also serializes every texture load with
  shader, photon-map, cache and 3D-texture loads.
- **Fix**: `CTextureSystem` locks its own find → load → insert by construction (one lock held across the load,
  so one name is loaded once and the diagnostic is reported once). The lock is
  `CTextureSystemSettings::loadMutex` when the creator supplies one, else a mutex the system owns. The renderer
  core passes `&CRenderer::shaderMutex` (from phase 6, below), so texture loads stay serialized with the other resource loads exactly
  as today. Without that, a texture synthesis (`globalArena`, guarded only by the synthesis lock, R13) could run
  concurrently with a `globalArena` user that holds only `shaderMutex`; the lock choice removes that new
  hazard instead of reasoning about each user. The services' `shaderMutex` wrapper around the three texture
  getters is deleted with the getters, and only in that step (phase 6) does the core start passing `shaderMutex`:
  it is a plain pthread mutex (`os.cpp:419-425`, `pthread_mutex_init(&mutex, NULL)`), so passing it while the
  wrapper still holds it around a forwarding getter (phase 5) would deadlock on the first texture. In phase 5 the
  system uses its own mutex. The other `shaderMutex` holders (`getCache`, `getTexture3d`, `getPhotonMap`, all in
  `rendererServicesImpl.h`) never reach a texture handle while holding it. **Core-lock coupling** (documented in the registry note and the
  CLAUDE.md gotcha): the system's lock is the core's non-recursive `shaderMutex`, so `handle()` carries a debug re-entry assertion
  and `TextureSystem_ConcurrentFirstLoadExternalMutex` tests the configuration.
- **Eviction is untouched**: `textureMemFlush`'s scan, LRU order and the per-block locks are moved, not changed
  (#19 stays out).
- **Test** (`TextureSystem_ConcurrentFirstLoad`, maintainer decision 5): 4 threads behind a start barrier call
  `textureHandle(name)` on one fresh system; 200 repeats, a fresh system each. A counting test resolver records
  how many times the name is located (one locate per load). Asserts: the four handles are equal, exactly one
  locate, exactly one `CODE_NOFILE` for a missing name (counting diagnostic sink, libRIBCommon). The resolver
  sleeps 1 ms inside `locate()` to hold the window open, so the red result does not depend on luck. Plain
  build, no ThreadSanitizer; registered in `tests/unit/texture_tile/CMakeLists.txt` with labels
  `texture_tile;unit`; run only as `cpujob … ctest -L texture_tile -j2`.
- **Red evidence**: (1) on `bc261b1` it fails to build (the API does not exist); (2) in phase 5 the system first
  receives the find-load-insert sequence moved verbatim from `rendererFiles.cpp` (no lock — the lock lived in
  the services wrapper), the test is enabled and **fails by assertion**, then the lock lands and it passes.
  This is the plan's reading of FR-018(c)'s "shown failing on the tree before the change"; see plan Q3.

## R7. Lifetime: per frame (clarification) against services per session

- **Decision**: `CRenderer::beginFrame()` constructs the system where `initTextures()` runs today
  (`renderer.cpp:750`: after the thread count is final, after `toWorld` is set, before the hiders' display
  setup and the contexts); `endFrame()` destroys it where `shutdownTextures()` runs today
  (`renderer.cpp:874`: after `frameFiles->destroy()`, after `endDisplays()`). `CRenderer::textureSystem` (a core
  static) holds it in between; `CRendererServicesImpl::textureSystem()` returns it, `nullptr` between frames.
- Destruction order changes only in where the texture objects die: today they are deleted inside
  `frameFiles->destroy()`, interleaved with the 3D objects in trie order; after, the 3D objects die first, then
  the system's cache. Texture destructors write nothing and print nothing (stats counters only), so no output
  can change.
- Imager and default shaders loaded between `RiBegin` and the first `RiWorldBegin` only load: no texture lookup
  runs outside a frame (lookups run during `render()`). A lookup that does find no system gets the no-system
  result (R11), never a stale pointer, because the pointer is cleared in `endFrame()`.
- Repeated `RiBegin`/`RiEnd`: every piece of texture state is inside the per-frame system, except the
  process-wide synthesis lock (R13), which holds no data.

## R8. File search with no renderer (FR-007)

- `CRenderer::locateFile` (`rendererFileSearch.cpp:71-140`) also maps network files, falls back to the temporary
  directory and fetches from the network client, so the core keeps it. Texturing gets an interface:

  ```cpp
  class CTextureFileResolver {
      public:
          virtual ~CTextureFileResolver() = default;
          // Full path of `name` into `result` (OS_MAX_PATH_LENGTH bytes); TRUE when found.
          virtual int locate(char *result, const char *name) const = 0;
  };
  ```
- The core implements it once (`CSearchPathResolver` in `src/ri/render/`, over a `TSearchpath *`, calling
  `CRenderer::locateFile(result, name, path)`). The frame's texture system gets one over
  `CRenderer::texturePath`; `RiMakeTexture`/`RiMakeShadow`/`RiMake*Environment`/`RiMakeBrickMap`
  (`rendererContext.cpp:6134-6185`) build one on the stack over `options->texturePath`. Messages
  (`CODE_RESOLUTION`) and results are therefore the same.
- `makeTexture`, `makeSideEnvironment`, `makeCubicEnvironment`, `makeSphericalEnvironment`,
  `makeCylindericalEnvironment` (`texmake.h:40-44`) and `makeBrickMap` (`brickmap.h:257`, and its friend
  declaration in `pointCloud.h:147`) take `const CTextureFileResolver *` instead of `TSearchpath *`. That removes
  every use of `options.h` from Texturing headers (`TSearchpath` was its only use) and `renderer.h` from
  `texmake.{h,cpp}` and `brickmap.cpp`. `writeTextureCacheEntry` passes the system's resolver to `makeTexture`
  (today `CRenderer::texturePath`, `texture.cpp:2834`; the source path is already absolute there).
- No renderer present: `otexmake` goes through `RiBegin(RI_NULL)` + `RiMake*` (`otexmake.cpp:197-249`), so it is
  covered by the core resolver. `ptcapi` and the data viewer construct 3D classes directly and never search.
  The Texturing-only test supplies its own resolver (a directory list).
- **Alternative rejected**: moving `TSearchpath` into libRIBCommon so Texturing could keep the old signatures.
  The search itself (network mappings, temporary path) is renderer behaviour and would still be out of reach.

## R9. The texture part of the services (FR-009, FR-011, FR-014)

- `CRendererServices` (`rendererServices.h:84-89`) has six resource getters; five return Texturing classes
  (`getTexture`, `getEnvironment`, `getTextureInfo`, `getCache`, `getTexture3d`), one a Hiders class
  (`getPhotonMap`).
- **Decision**:
  - add `virtual CTextureSystem *textureSystem() = 0;` (the frame's system, `nullptr` between frames);
  - remove `getTexture`, `getEnvironment`, `getTextureInfo` (their cache moved, R5);
  - keep `getCache`, `getTexture3d`, `getPhotonMap` unchanged: their `frameFiles` entries stay in the core
    (FR-014), as do the remote-channel registrations they make;
  - add three forwarders for the 3D channel operations of `texture3d()`/`bake3d()`, implemented by the core:
    `texture3dResolve` (returns the data size, replacing the separate `dataSize` field read),
    `texture3dLookup`, `texture3dStore`. They call `CTexture3d::resolve`/`lookup`/`store`, whose signatures do
    not change (FR-014).
- **Why the 3D forwarders are on the services and not on the system**: a `CTextureSystem` virtual that calls
  `CTexture3d::resolve` puts `texture3d.o` into every link of the system, and `texture3d.cpp` references
  `CRenderer::retrieveDisplayChannel` (FR-015), so the Texturing-only program (US1, R15) could not link without
  the core. On the services they are core code calling Texturing, which is allowed.
- **Flat imports**: Shading calls the system and the services through virtual calls only, and `TextureOpt`,
  `TextureSampleStream` and the handle types are header-only aggregates with no out-of-line member, so no
  Texturing symbol is imported. Routing `resolve()` through the services removes the one Texturing import:
  **7 → 6** (SC-004). The Linux counterpart is measured by spec 024's method (quickstart §5).
- **Reading of FR-009** ("replacing the five resource getters"): with FR-014 keeping the cache and 3D entries
  in the core, the plan replaces the three texture-family getters and keeps `getCache`/`getTexture3d` until
  032; see plan Q2. The two irradiance-cache lookups Shading makes with its context
  (`giFunctions.h:235`, `shading.cpp:3394`, `cache->lookup(C, P, dPdu, dPdv, N, this)`) are calls of
  `CTexture3d`'s six-argument lookup, which FR-014 leaves to 032; they stay, as SC-005's "apart from the 3D
  lookup" exception.

## R10. The Shading call sites (FR-009, FR-010)

| Builtin | Interpreter (`shaderFunctions.h`, `giFunctions.h`) | JIT (`shading.cpp`, called from `op_*` in `rslOps.cpp`) |
|---|---|---|
| `texture()` (4 forms) | `TEXTURE*EXPR`, `lookup4` on the derivative footprint, after `plReady()` (`1566-1780`) | `jitTextureF`/`jitTextureC`: **point** `lookup(tmp, s, t)` (`2970-2999`) |
| `environment()` | `ENVIRONMENTEXPR`, `lookup(D0..D3)` on the footprint, `"raytrace"` traced (`1806-1910`) | `jitEnvironmentF`/`C`: `lookup(d, d, d, d)` (`3001-3034`) |
| `shadow()` | `SHADOWEXPR`, footprint (`~2000-2030`) | `jitShadowF`: `lookup(p, p, p, p)` (`3036-3053`) |
| `textureinfo()` | `TEXTUREINFO_PRE` (`1393-1510`) | `jitTextureInfo` (`3690-3750`) |
| `texture3d()`, `bake3d()` | `resolve`, `dataSize`, `lookup`/`store` (`2160-2326`) | `jitTexture3d`, `jitBake3d` (`3441-3520`) |
| occlusion/indirectdiffuse environment | `giFunctions.h:202`, stored in `occlusionParams.environment` for the hider | `shading.cpp:3347` |
| scalar helper | `rsl_texture_c_scalar` (`rslBuiltins.cpp:178-191`), no caller | — |
| hider | `irradianceDispatch.cpp:405,545` (`tex->lookup(…, context)`) | — |

- **Decision**: one Shading unit, `src/libshader/shading/textureCalls.{h,cpp}` (beside `serviceLookup`), owns
  every texture request: handle acquisition with the no-system rule (R11), `TextureOpt` construction from the
  scratch and the draw cursor (`CShadingContext::textureOptions()`), the cursor write-back
  (`commitTextureDraws()`), and `lookupEnvironment(handle, result, D0..D3)` for the hiders. The interpreter
  macros, the `jit*` functions, `rsl_texture_c_scalar` and `irradianceDispatch.cpp` call it; none calls a
  Texturing lookup method or passes a context to Texturing (FR-009, SC-005).
- `environment()`'s choice of ray tracing is made **by name** (`"raytrace"` or the shader's own name,
  `shaderFunctions.h:1819`), as today, and no longer also by a null result: with a system present the getter
  never returned null, and with none the builtin must return the fill value (R11), not trace.
- **Finding: the two backends do not request the same lookup today.** The JIT has no parameter list for
  `texture()`/`environment()`/`shadow()` (`llvmEmitter.cpp:2690-2819` passes only positional operands), calls
  the point lookup or a zero-width footprint, and its options are whatever an earlier call left in the scratch.
  This is the #55 class of defect for the texture builtins. FR-010 ("both backends hand the services the same
  handle request, option block and coordinates, and produce the same result") cannot hold while FR-020 keeps
  `.slo` renders byte-identical. The behaviour-preserving phases keep each backend's request exactly (the JIT
  builds its `TextureOpt` from the same scratch it reads today) and the substitute test pins what each sends; the
  maintainer's answer to Q1 (2026-10-27) fixes the JIT in a final, output-changing phase (R22).
- The JIT `op_*` entry points keep their signatures, so `.slo` bitcode does not change and no shader rebuild is
  needed beyond the normal dependency tracking.

## R11. No texture system installed (FR-002, FR-012, maintainer decision 2)

- `textureCalls` returns a null handle when there are no services or `textureSystem()` is `nullptr`, and warns
  once per kind of lookup per process (`std::atomic<bool>` per kind, `CODE_NOTSTARTED`, the `serviceLookup.cpp`
  convention): kinds `texture`, `environment`, `shadow`, `textureinfo`, `texture3d`, `bake3d`.
- A lookup with a null handle writes the fill value (`TextureOpt::fill`) into each channel it would write and
  returns; `shadow()` returns the fill value too; `textureinfo()` returns 0 and writes nothing (as for a missing
  file today); `texture3d()`/`bake3d()` with no services write nothing and return 0 (as the JIT does today for a
  null `rendererGetTexture3d`).
- Today with no services the interpreter dereferences a null `CTexture*` (`shaderFunctions.h:1620`) and the
  JIT returns early leaving the result unset; neither happens in a render with services, so no render changes.

## R12. The point hierarchy's options (FR-014)

- `CPointHierarchy::lookup` overrides `CTexture3d`'s six-argument lookup, whose signature FR-014 keeps, and is
  called only by the irradiance cache in the hiders (`irradiance.cpp:320`), with the context. It dereferences
  the context for one value: `scratch.occlusionParams.maxSolidAngle` (`pointHierarchy.cpp:474-475`), the only
  reason `pointHierarchy.cpp` includes `shading.h`.
- **Decision**: `CPointHierarchy` takes a `CPointHierarchy::Options` at construction:
  `{ float (*maxSolidAngle)(const CShadingContext *context); }`, where `CShadingContext` stays an opaque forward
  declaration in Texturing. The core constructs point hierarchies (`rendererFiles.cpp:701`) and passes a
  function it defines (core may read Shading's scratch). `lookup()` calls it with the context it already
  receives. Values and call order are unchanged; the include goes; no hider code changes.
- **Alternatives rejected**: changing the six-argument lookup's last parameter to a Texturing options type
  (FR-014 keeps that signature for 032); a new non-virtual `CPointHierarchy` method called from
  `irradiance.cpp` (a new Hiders → Texturing link edge, FR-019); a thread-local the hider sets (hidden state);
  passing the angle through the unused `dPdu`/`dPdv` arguments (a misuse of the interface).

## R13. The pyramid-synthesis lock (FR-001, FR-008)

- `CRenderer::synthesizeMutex` (`rendererMutexes.cpp:160`) serializes `buildSynthesizedPyramid`
  (`texture.cpp:699-723`) because it uses `globalArena`, an unsynchronized process-wide bump allocator in
  libRIBCommon. `createSynthesizedTileSource(filename, level)` (`texture.cpp:825`) takes it too, and FR-008 keeps
  that function and its direct unit test (`TileSource_SynthesizedInfo`) unchanged, so the lock must exist with no
  texture system.
- **Decision**: a Texturing-owned, process-wide `std::mutex` (function-local static in `texture.cpp`, so it needs
  no init or teardown and holds no data); the system uses it. A per-system mutex would not protect a direct
  `createSynthesizedTileSource` call, and the resource it guards is process-wide. The block-registry and
  eviction mutex (today `CRenderer::textureMutex`) does become the system's own (`CTextureCache`), since the
  block list is per system. FR-001 is worded accordingly (maintainer, 2026-10-27, Q4).
- **Alternative**: a `globalArena` lock in libRIBCommon itself (most principled, since that is the shared
  resource), at the cost of a libRIBCommon change and of deciding which other `globalArena` users must take it.
- `texture.cpp` is compiled into both `ribRender` and `ribVector` (`ribVector_sources` is `ri_sources` minus a
  list that keeps the Texturing sources, `src/ri/CMakeLists.txt:354-375`), so a process loading both would hold
  two copies of this lock, exactly as it holds two `CRenderer::synthesizeMutex` today (`rendererMutexes.cpp` is
  in both). `ribVector` is linked only by `libribpreview` (`src/preview/libribpreview/CMakeLists.txt:39`), which
  does not load `ribRender`.

## R14. The RGBE codec (FR-017, maintainer decision 3)

- Today `src/display/rgbe/rgbe.{h,cpp}` is compiled into the `rgbe` display module (`file(GLOB …)`,
  `src/display/rgbe/CMakeLists.txt`) and, as `../display/rgbe/rgbe.cpp`, into `ri_sources`
  (`src/ri/CMakeLists.txt:67`), hence into `ribRender` and `ribVector`. The codec holds no state: only `static`
  helpers and the error printer.
- **Decision**:
  - move it to `src/common/rgbe.{h,cpp}` (classified Infra by `domain-map.txt`), add `rgbe.cpp` to
    `openrendercommon_obj`, remove it from `ri_sources`;
  - declare the six functions `extern "C"` in `rgbe.h`, so the symbols are `RGBE_ReadHeader`, … on both
    platforms instead of mangling `FILE *` as `__sFILE` (macOS) or `_IO_FILE` (glibc);
  - the `rgbe` module's glob then finds only `file.cpp`; it already links the shared `openrendercommon`
    through `openrenderfilebase` (`PUBLIC`, `src/display/file/CMakeLists.txt:5`), so it resolves the codec from
    the one shared copy, as decided;
  - `imageInputRgbe.cpp` and `file.cpp` include `common/rgbe.h`.
- **`Common_StateOwnership`**: the image list already contains `ribRender`, `ribVector`, `rgbe` and every other
  image (`tests/unit/common/CMakeLists.txt:23-29`); it does not change. `owned-symbols.txt` gains a spec-027
  section with the six unmangled names. Because they are `extern "C"`, one line per symbol is right on both
  platforms; the script has no platform tags and fails on a symbol no image defines
  (`state_ownership.cmake`, "NOT DEFINED"), so mangled C++ names could not be listed for both platforms.
- **Alternative**: keep C++ linkage and add platform tags to `owned-symbols.txt` (a format change to a spec-023
  tool for one entry).
- **No test drives the RGBE display driver today.** `ctest -N` lists no test that writes through `rgbe.dsply`;
  US4 AS2 presumes one. The plan adds `Display_RgbeBytes` (written before the move, green on both trees by
  design): a small deterministic scene displayed through `"rgbe"`, its bytes compared with a reference written by
  the starting tree's driver.
- Ordering: the move is independent of the texture system (the spec's "can ship alone"); it is phase 3.

## R15. The Texturing-only program (FR-018a, US1, SC-003)

- **What can stand alone**: the 2D family and its tools: `texture.cpp`, the new `textureSystem.cpp`,
  `texmake.cpp`, `imageInput*.cpp` (and `common/rgbe.cpp` through libRIBCommon). The 3D family cannot:
  `texture3d.h` derives `CTexture3d` from the data viewer's `CDataView` (DB-025, 032's) and `texture3d.cpp`
  reaches the channel registry through `CRenderer::retrieveDisplayChannel` (DB-021, FR-015). US1 AS4 already
  excepts those edges; the program covers the 2D family.
- **Target**: `texturing_only_sources` (static library of those sources compiled a second time with a narrow include
  path: `src`, `src/ri/texture`, `src/common`, a staged copy of `src/ri/core/ri_config.h`, the build's
  `config.h`; never `src/ri/state`, `src/ri/render`, `src/ri/core` or `src/libshader`), linking only
  `openrendercommon`, libtiff, libpng, zlib and OpenEXR when present. Executable `TextureOnly_Standalone` (label
  `texture_only;unit`): opens a baked and an unbaked fixture, looks up with a caller-built `TextureOpt` and
  `TexturePerthread`, compares with the recorded values.
- `TextureSystem_ConcurrentFirstLoad` (R6) and `TextureSystem_StreamCursor` (R16) need no renderer either, but
  they must run in phases 5 and 6, before the option is on. They link `ribRender` like `TileSource_TiffInfo`
  (`tests/unit/texture_tile/CMakeLists.txt`), construct the system with `createTextureSystem` and a test resolver,
  and never call `RiBegin`. Until their phase they are `EXCLUDE_FROM_ALL` (the red build is shown by building the
  target explicitly); `tests/` is not an archcheck domain, so this does not affect the check.
- **archcheck**: it walks every `DependInfo.cmake`, `EXCLUDE_FROM_ALL` included, and exits 2 on a missing in-scope
  object, so the target exists only behind `OPENRENDER_TEXTURE_ONLY_TESTS`, off until phase 8 switches it on.
  Spec 025 measured the same arrangement for State (025 validation.md, "Research R11 (first run …)"): doubly
  compiled units leave every deduplicated number unchanged and a staged `ri_config.h` is not reported.
- **"Values recorded from master"**: the starting tree cannot run a lookup without a shading context, so the
  values are recorded through the renderer. A probe scene (`tools/lookup-probe.rib` + `tools/lookupprobe.sl`)
  calls the **eight-coordinate `texture()` only** on a baked and an unbaked fixture, at fixed footprints with
  `"samples" 1` (jitter 0, so the stream does not affect the value) and a box filter (`environment()` and `shadow()` take
  one direction/point and Shading derives their footprint, which the Texturing-only program would have to invent; T-K1,
  the substitute test and the renders cover them), and
  `printf`s each result; RSL `printf` formats with `%f` (`scriptFunctions.h:709-733`). The values are saved as
  `tools/master-lookup-values.txt`; the standalone program formats its own results with `%f` and compares the
  strings.

## R16. Test design details

- **Substitute texture system** (`ShadingParity_TextureSubstitute` for the 2D, info and no-system parts and `ShadingParity_TextureSubstitute3D` for the `texture3d()`/`bake3d()` forwarders, a separate executable because the forwarders enter the interface only in phase 7; FR-012, FR-018b, US3, SC-008): the
  `shading_parity` in-process pattern of `test_services_routing.cpp`. After `RiBegin` the test installs a
  `CForwardingServices` (`tests/shading_parity/table_services.h`) whose `textureSystem()` returns a recording
  substitute (canned values, every call logged with handle request, kind, `TextureOpt` fields and
  coordinates). One probe shader per builtin is compiled to `.rslo` and `.slo` with `OSHADER_BIN` and rendered
  on a tiny grid. Checks: each backend's log equals that backend's expected request (R10: the interpreter's
  footprint and parameter-list options; the JIT's point or zero-width request and scratch options); the handle
  requests (name, kind) and `textureinfo()`/3D requests are equal across backends; every builtin returns the
  canned value; and, with the substitute removed (`textureSystem()` returns `nullptr`), each builtin returns its
  fill value and each kind warns exactly once (R11). Registered only when `OPENRENDER_ENABLE_JIT` is on.
- **Stream cursor** (`TextureSystem_StreamCursor`, FR-005, FR-018d): no renderer, no context (R15). For each lookup class in
  R3's table, a `TextureOpt` with a known stream and sample counts; asserts the dimensions consumed equal the
  table, that a lookup on an environment whose side is a texture consumes the side's dimensions from the same
  stream, and that `stream.dim` is visible to the caller afterwards. "Equals master's": the table is derived
  from the starting tree's code; the end-to-end proof is the byte-identical renders.
- **JIT texture probes**: no `-slo` test in the suite renders `texture()`, `environment()` or `shadow()` (the
  `Parity_texture-*`/`Parity_env`/`Parity_shadow` scenes use the default `rslo`), so the image manifest would not
  see a JIT routing regression. `tools/jit_texture_probes.sh` renders the texture, environment, shadow and
  unbaked parity scenes with `Option "shaderformat" "slo"` at `-t:1` into the build tree, so `image_manifest.sh
  record` hashes them with the suite's images (baseline in phase 1, compared in every phase). `-t:1` because the
  JIT's options are scratch leftovers (R10), which at the default thread count depend on which context shades which
  bucket; phase 1's noise run confirms the probes are stable.
- **Characterization**: `Display_RgbeBytes` (R14).

## R17. Allowlist, metric and registry end state

`contracts/allowlist-end-state.md` lists every line. In short:

| Phase | Lines that go `STALE` and are deleted |
|---|---|
| 3 (RGBE) | the 5 DB-026 lines (3 evaluated on macOS, the 2 `linux` lines on Linux) |
| 4 (resolver, State includes) | 13 DB-010: includes `brickmap.cpp → renderer.h`, `brickmap.h → options.h`, `pointCloud.h → options.h`, `pointHierarchy.h → options.h`, `texmake.cpp → renderer.h`, `texmake.h → renderer.h`, `texmake.h → options.h`, `texture.h → attributes.h`, `texture3d.h → xform.h`; links `CRenderer::locateFile` from `brickmap.cpp`, `texmake.cpp`, `texture.cpp`, and `CRenderer::texturePath` |
| 5 (system ownership) | 10 DB-010: include `texture.cpp → renderer.h`; links `numThreads`, `textureMutex`, `synthesizeMutex`, `textureMaxMemory`, `textureRefNumber`, `textureUsedBlocks`, `textureUsedMemory`, `textureCacheEnabled`, `textureCacheDirectory` |
| 6 (lookups) | 1 DB-010: include `texture.cpp → core/shading.h` |
| 7 (3D through services) | 1 DB-010: include `pointHierarchy.cpp → core/shading.h` |
| 10 (registry) | re-tag `texture3d.cpp → renderer.h` and `texture3d.cpp → _ZN9CRenderer22retrieveDisplayChannelEPKc` DB-010 → DB-021 |

- Total: 25 DB-010 lines and 5 DB-026 lines deleted, 2 re-tagged, none added. DB-010 35 → 8 (the Hiders-origin
  lines, whose keys the plan never changes: `texture.h`, `texture3d.h`, `pointHierarchy.h` keep their paths and
  `CTexture3d`'s constructor, destructor and type information their names). DB-021 1 → 3. DB-025 unchanged.
- **Platform variants of the re-tag (maintainer decision 1)**: neither line carries a platform tag, and neither
  symbol mangles differently on glibc (no `FILE *` parameter), so each is one untagged line covering both
  platforms. The Linux run (R20) confirms archcheck reports no Linux-only variant; if one appeared it is
  re-tagged the same way.
- **Metric**: Texturing 70/16 → 1/1 (`retrieveDisplayChannel`). TOTAL refs 721 → 652 (−69), at `bc261b1`; re-measured on `BASE` (T007, after spec 025 is merged in) TOTAL is still 721 refs / 110 distinct, so the figures stay 652 / 98 (the same −69 / −12 are applied to the re-measured TOTAL). TOTAL distinct is
  not a row sum (spec 024's note): of the 16 members, `locateFile`, `numThreads` and `texturePath` are still
  referenced from Geometry and `numThreads` from Hiders, and `retrieveDisplayChannel` from Texturing, so 12
  members leave TOTAL: 110 → 98 expected, written as archcheck reports it.
- **Registry**: DB-026 → Resolved (spec 027, date of the phase); DB-010 title, evidence, size (35 → 8) and
  direction edited to the GI part left to 032; DB-021 evidence and size (1 → 3) edited; DB-001's symbol list
  loses `CTexture3d::resolve` (7 → 6 imports), its link-edge count is unchanged (that import was never an edge).
  No entry refers to the merge.

## R18. Phase order and behaviour preservation

Each phase is behaviour-preserving and ends with the same checkpoint (plan, "Checkpoints"): clean `-Werror`
build, the suite against the phase-1 pass list, `ctest -L architecture` green with the phase's lines deleted
first (red until the edges go), and the image manifest identical to the baseline, JIT probes included. Lines are
deleted at the start of the phase that removes their edges, as spec 025 does, so the phase's red is visible.

Why this order:
1. The RGBE move first: independent, smallest, resolves one entry.
2. The resolver before the system: the loaders need a resolver to stop calling `locateFile`, and the
   `options.h` includes go with `TSearchpath`.
3. Ownership before signatures: moving the state while the lookups still take the context keeps each diff
   reviewable; the textures reach their per-thread state through their owning cache and `context->thread` in
   phase 5, through `TexturePerthread` from phase 6.
4. The 3D forwarders and the point hierarchy after the 2D lookups: they touch 032-adjacent code and the flat
   imports, and nothing earlier depends on them.
5. The Texturing-only option after every Texturing edge is gone.
6. The JIT parity fix (R22) last among the code phases: it is the only phase allowed to change output (`.slo` only),
   so it follows every behaviour-preserving phase and its image changes cannot hide a regression from them.
7. The registry, docs and the Linux run.

## R19. Performance (SC-010)

Method (phase 1 and phase 10): `concurrency-scene.rib` (the `texture_tile` concurrency scene, heaviest texture
load in the suite) and `parity/texture-png-raytrace.rib`, 10 runs each at the default thread count and `-t:1`, wall
time through `cpujob`, on the same machine. Phase 1 records them, on `BASE`, as `tools/timing-base-macos.tsv`; phase 10 runs
the same script on the finished tree and compares with that file (quickstart §0.7 and §3). Interleaving a starting
and a finished `orender` is not used: a copied `orender` binary still links the build tree's own shared libraries
(`libRIBRender`, `libRIBCommon`), so it would not stay the starting code. The run-to-run spread of the baseline file
is the noise bar; a finished-tree median outside it is a defect to report, not a budget to
widen. The added work per lookup is building one struct and one virtual call per builtin call.

## R20. Linux

Not available to the plan agent. The quickstart lists the maintainer's rsync + Maestri run: build with GCC 13
`-Werror`, the full suite (expected failures #45 and #46: `TextureTile_RenderByteIdentical` and the four
`TextureTile_Cache*`), `ctest -L architecture` (the 2 `linux` DB-026 lines evaluated there; confirm no
Linux-only variant of the two re-tagged lines), the Linux flat-import counterpart (spec 024's `nm -D` + `U`
intersection), `Common_StateOwnership` with the `extern "C"` RGBE names. Once at the start (starting numbers,
decision 4) and once at the end.

## R21. Left alone, recorded

- `src/ri/hiders/radiance.cpp` is **not compiled** (absent from `src/ri/CMakeLists.txt`), and its environment
  call (`radiance.cpp:1150`) already does not match today's signature (it passes a `CTextureLookup *` where a
  `CShadingContext *` is expected). Left untouched and dropped from FR-013 (maintainer, 2026-10-27, Q5).
- `rsl_texture_c_scalar` (`rslBuiltins.cpp:178`) has no caller and the emitter never names it. It is routed
  through `textureCalls` like the others (FR-009 names `rslBuiltins.cpp`); deleting it would be a separate
  decision.
- The GI getters (`getPhotonMap`, `getCache`, `getTexture3d`) keep their unlocked find-load-insert on
  `frameFiles`, still serialized only by the services' `shaderMutex` wrapper; that is 032's cache.
- The within-family name collision (R5): GitHub #104, out of scope.
- `remoteChannel.cpp:184` calls `getTexture3d` directly; untouched (3D, network).

## R22. The JIT parity phase (FR-022, maintainer answer to Q1)

**What is wrong today** (read from `llvmEmitter.cpp:2690-2819` and `shading.cpp:2971-3054`, confirmed on a probe
shader compiled with the fresh `oshader`):

| Defect | Evidence |
|---|---|
| The optional parameter list is dropped | the emitter passes only the positional operands; `jitTexture*`/`jitEnvironment*`/`jitShadowF` never bind a `CTextureLookup`/`CEnvironmentLookup`, so `blur`, `width`, `swidth`, `twidth`, `fill`, `samples`, `filter`, `bias` are ignored and the options are whatever an earlier call left in the scratch |
| The form is chosen by operand count, and the IR always carries the channel operand | the `.rslo` instruction is `texture ("c=SFff") a tn 0 s t …`: the color form also has the channel (`0`). The emitter takes the float-channel branch whenever there are ≥ 4 (`texture`) or ≥ 3 (`environment`) operands, so a **color** `texture()`/`environment()` calls `op_texture_f_v2`/`op_environment_f_v2` and writes one channel (a probe's `.slo` references `op_texture_f_v2` and `op_environment_f_v2` and never the `_c_v2` entry points); `shadow()` has one float entry point for both forms |
| No filter footprint | `texture()` calls the point `lookup(s, t)`, `environment()`/`shadow()` pass `D, D, D, D`/`P, P, P, P`; the interpreter builds the four corners from the coordinates' grid derivatives, `du`/`dv` and `swidth`/`twidth` |
| The eight-coordinate `texture()` uses only the first corner | `s`, `t` are operands 2 and 3; the other six are dropped (and with a parameter list after them, the name tokens are misread as values) |
| **The `shadow` emitter branch passes the wrong operand as `Ps`** (found at Checkpoint 6, Phase 6) | `llvmEmitter.cpp` (`op == "shadow"`) takes `getVar(ins, 1)`, the **channel** operand, as `Ps`; the point is operand 2. `op_shadow_f_v2` therefore reads the memory next to the channel as the point (the substitute test records `P = 0, 8.4e-39, 1.4e-45`). The `shadow` parity scenes are identical only because they do not exercise it |
| **`textureinfo()` under the JIT returns garbage resolution values** (found at Checkpoint 6, Phase 6) | the substitute test's `.slo` `textureinfo` probe reads `found = 1` but resolution values such as `3.2e-35, 1.4e-45`; `jitTextureInfo`'s own logic is the interpreter's. The cause (the emitter's operand or destination handling for the array result) is to be **traced in Phase 9** |
| `environment("raytrace")`/`shadow("raytrace")` are looked up as files | the interpreter traces (`ENVIRONMENTEXPR_PRE`, `shaderFunctions.h:1819`); the JIT asks for an environment map named `"raytrace"`, gets the substitute and the `"Failed open environment"` error (plan Q7) |

**Measured divergence** (`tools/jit_texture_divergence.sh`, `-t:1`, each parity scene rendered with the default
`.rslo` shaders and with `Option "shaderformat" "default" ["slo"]`; `tools/jit-texture-divergence-bc261b1-macos.tsv`):
16 of 18 scenes differ — `texture-{png,exr,rgbe}` 34.55–252.52 MaxBlockAvgDiff, `unbaked-*` 73.45–156.39,
`env` 244.11/244.14; `shadow-{reyes,raytrace}` are identical (`shadowspot` passes no parameter, samples 1, blur 0).

**Which existing outputs change: none.** Of the 60 scenes that select `.slo` (`Attribute "shade" "shaderformat"
["slo"]` or `Option "shaderformat" "default" ["slo"]`) none binds a shader that calls `texture()`,
`environment()` or `shadow()`, and no test shader in `tests/`, `src/` or `examples/` calls them. So no `Visual_*`,
`Parity_*`, `ShadingParity_*` or `DeterminismSweep_*` result and no stored reference is expected to change; the only
images that change are this spec's own JIT texture probes (`tools/jit_texture_probes.sh`, not references), which
must then equal their `.rslo` renders. Any other change found in that phase stops it (a STOP task for the
maintainer), with the scene, old and new diff and reason listed; no re-render without approval, no threshold change.

**Design** (#55's mechanism, nothing new):
- **Emitter**: the form comes from the instruction's prototype (`"c=SFff…"`, `"f=SFffffffff…"`, `"c=SFv…"`,
  `"f=SFp…"`), not the operand count; the positional operands are those of the prototype; the rest is the
  parameter list, marshalled by `emitParamList()` (odd lists, non-literal names and unresolvable values fail the
  emit with a diagnostic, as for #55). `du`, `dv` (and for the footprint, nothing else: the derivatives of the
  coordinates are taken on the grid at run time, as `jitTexture3d` does with `duVector`) are passed as grid pointers
  like `op_trace_*_v3`.
- **Entry points**: `op_texture_{f,c}_v3`, `op_texture_full_{f,c}_v3` (eight coordinates), `op_environment_{f,c}_v3`,
  `op_shadow_{f,c}_v3`, each with `names, values, steps, isVarying, numPairs` and the call site; the `_v2` names are
  deleted, so a `.slo` compiled before the fix fails to bind instead of silently making the old request (#55's
  rule). `rslOps.h` and the JIT ABI contract list them.
- **Run time**: `jitParameterList<CTextureLookup>()` (`texture()`) or `<CEnvironmentLookup>()` (`environment()`,
  `shadow()`) binds the list once per (instance, call site) through the interpreter's own `bind()`/`init()`/
  `postBind()`; per vertex `jitParameterListReady()` copies the varying values into the scratch; then the same
  footprint arithmetic as `TEXTURE*EXPR`/`ENVIRONMENTEXPR`/`SHADOWEXPR` (moved into one helper both backends call,
  so they cannot drift), `textureOptions()`, the texture system, `commitTextureDraws()`. The traced names follow
  plan Q7.
- **Tests, red first** (`tests/visual/CMakeLists.txt`, extending #55's table; probes
  `shaders/jit_pl_texture_probe.sl` — red `texture()` color, green `texture()[1]` float, blue the eight-coordinate
  form — and `shaders/jit_pl_envshadow_probe.sl` — red `environment()` color, green `environment()` float, blue
  `shadow()`; scenes generated from `examples/rib/tests/jit-texture-probe.rib.in`, every parameter value a shader
  parameter set in the RIB):
  - `ShadingParity_JitPL_texture_<case>` and `ShadingParity_JitPL_envshadow_<case>`: the case under `.rslo` and
    `.slo`, bit-identical. Cases: `base`, `blur`, `width`, `swidth_twidth`, `fill` (a missing map), `samples`,
    `filter` (texture); `base`, `blur`, `width`, `fill`, `samples`, `bias`, `filter` (environment and shadow).
  - `ShadingParity_JitPLEngaged_<probe>_<case>_{rslo,slo}`: the base scene against the case, same format, on the
    channels the case affects, must differ; not registered for a parameter the interpreter itself ignores for that
    builtin (decided from the code when the table is written, as #55 did).
  - Red: before the fix every `JitPL_texture_*`/`JitPL_envshadow_*` parity case fails (each probe has a texture or
    environment channel, which differs even with no parameter: the measured divergence), and every `_slo` engaged
    control fails (the JIT ignores the parameter).
  - `ShadingParity_JitPLDiagnosticsTexture`, a new test (the existing `ShadingParity_JitPLDiagnostics` stays as it is,
    green until and after phase 9), covers the three builtins (an unknown parameter is reported once, as by the
    interpreter).
