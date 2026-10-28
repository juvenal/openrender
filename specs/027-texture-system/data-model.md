# Data Model: Texture System (spec 027)

The entities of the texture system, who owns each, and how they move through a frame. Interfaces are in
[contracts/texture-system.md](contracts/texture-system.md) and
[contracts/renderer-services.md](contracts/renderer-services.md); the reasoning is in [research.md](research.md).

## 1. Ownership at a glance

| Entity | Domain | Header | Owned by | Lifetime |
|---|---|---|---|---|
| `CTextureSystem` (abstract) | Texturing | `src/ri/texture/textureSystem.h` (new) | — | interface |
| `CTextureSystemImpl` | Texturing | `src/ri/texture/textureSystem.cpp` (new, file-local) | the renderer core (`CRenderer::textureSystem`) | one frame |
| `CTextureCache` (tile cache) | Texturing | `src/ri/texture/textureInternal.h` (new, Texturing-private) | the system | one frame |
| `TexturePerthread` | Texturing | declared in `textureSystem.h`, defined in `textureInternal.h` | the system's cache (array of `numThreads`) | one frame |
| `TextureHandle` | Texturing | `textureSystem.h` (incomplete type) | the system's family cache | one frame |
| `TextureOpt`, `TextureSampleStream` | Texturing | `textureSystem.h` | the caller (stack) | one lookup |
| `TextureInfo` | Texturing | `textureSystem.h` | the caller (stack) | one `textureinfo()` |
| `CTextureSystemSettings` | Texturing | `textureSystem.h` | the creator (copied by the system) | construction |
| `CTextureFileResolver` (abstract) | Texturing | `src/ri/texture/textureResolver.h` (new) | — | interface |
| `CSearchPathResolver` | renderer core | `src/ri/render/searchPathResolver.h` (new) and `rendererFileSearch.cpp` | the core (one per frame for the system; one per `RiMake*` call on the stack) | as owner |
| `CPointHierarchy::Options` | Texturing | `src/ri/texture/pointHierarchy.h` | each `CPointHierarchy` (copy) | the hierarchy |
| RGBE codec | Infra | `src/common/rgbe.{h,cpp}` (moved) | libRIBCommon | process |
| texture part of `CRendererServices` | Shading | `src/libshader/shading/rendererServices.h` | implemented by `CRendererServicesImpl` (core) | session |
| `textureCalls` helpers | Shading | `src/libshader/shading/textureCalls.{h,cpp}` (new) | — | stateless but for the warn-once flags (process) |

`CTextureSystem` replaces, in the renderer core: `textureRefNumber`, `textureUsedBlocks`, `textureUsedMemory`,
`textureMaxMemory` (`rendererStatics.cpp:270-273`), `textureMutex`, `synthesizeMutex` (`rendererMutexes.cpp`),
`textureCacheEnabled`, `textureCacheDirectory` (`rendererStatics.cpp:145-146`), the members `textureLoad`,
`environmentLoad`, `getTexture`, `getEnvironment`, `getTextureInfo`, `initTextures`, `shutdownTextures`
(`renderer.h:283-298`), and the texture-family entries of `frameFiles`. `CRenderer::texturePath`,
`maxTextureSize` and `numThreads` stay: they are option values the core passes in.

## 2. `CTextureSystemSettings`

| Field | Type | Source in the core (`beginFrame`) | Today |
|---|---|---|---|
| `numThreads` | `int` | `CRenderer::numThreads` after the network-client reduction | `initTextures` reads it |
| `maxMemory` | `int` | `CRenderer::maxTextureSize` (`Option "limits" "texturememory"`) | `initTextures(maxTextureSize)`; per thread `ceil(maxMemory / numThreads)` |
| `resolver` | `const CTextureFileResolver *` (not owned) | a `CSearchPathResolver` over `CRenderer::texturePath` | `CRenderer::locateFile(…, texturePath)` |
| `diskCacheEnabled` | `bool` | `o->textureCacheEnabled` | `CRenderer::textureCacheEnabled` |
| `diskCacheDirectory` | `const char *` (copied) | `o->textureCacheDirectory` (`nullptr` = resolved default) | `CRenderer::textureCacheDirectory` |
| `toWorld` | `matrix` (copied) | `CRenderer::toWorld` | `environmentLoad(…, toWorld)` for shadow and deep-shadow maps |
| `loadMutex` | `TMutex *` (not owned, may be `nullptr`) | `&CRenderer::shaderMutex` from phase 6 (`nullptr` in phase 5, while the services wrapper still holds it) | the services wrapper's lock (research R6) |

Validation: `numThreads >= 1`, `resolver != nullptr`; otherwise `createTextureSystem` returns `nullptr` and
reports `CODE_BUG`.

## 3. `TextureHandle` and the family cache

- **Key**: the name, exactly as today; one entry per name for the whole texture family (research R5).
- **Value**: a `CTextureInfoBase *` (the existing base of `CTexture` and `CEnvironment`), returned behind the
  opaque `TextureHandle *`. Equal names give equal handles for the system's lifetime.
- **Kinds** (`ETextureKind`): `Texture` (2D), `Environment` (environment, shadow and deep-shadow maps),
  `Info` (`textureinfo()`).

State of one name in one system:

```text
absent ──handle(name, kind) under the load lock──▶ loading ──▶ present(real)
                                                        └──▶ present(substitute)   kind Texture/Environment, file missing or unreadable
                                                        └──▶ absent               kind Info, file missing: nothing inserted, next call probes again
present ──system destroyed (endFrame)──▶ freed
```

| Kind | Load order | On failure | Diagnostic (unchanged text) |
|---|---|---|---|
| `Texture` | 2D load (baked TIFF, else unbaked source via `CImageInput`, disk cache if enabled) | `CDummyTexture` cached | `CODE_NOFILE "Failed open texture \"%s\"\n"`, not for an empty name |
| `Environment` | deep shadow, else TIFF environment/shadow by `TIFFTAG_PIXAR_TEXTUREFORMAT` | `CDummyEnvironment` cached | `CODE_NOFILE "Failed open environment \"%s\"\n"` |
| `Info` | environment, then 2D | `nullptr`, nothing cached | none |

A substitute returns the lookup's `fill` in every channel, as the dummies do today. Because the load lock is held
across the load, a missing name is reported once per system (FR-002, FR-016).

## 4. `TextureOpt` and `TextureSampleStream`

Built by the caller at the call site, after `plReady()`, from `CShadingScratch` and the draw cursor; passed to
the lookup by reference; the caller writes `stream.dim` back afterwards (research R3).

| Field | Type | Filled from | Read by (today's read in brackets) |
|---|---|---|---|
| `filter` | `RtFilterFunc` | `textureParams.filter` (the call site sets it from the lookup's bound filter first, as today) | every filtered lookup (`textureParams.filter`) |
| `blur` | `float` | `textureParams.blur` | 2D, shadow, deep shadow, cubic environment |
| `width`, `swidth`, `twidth` | `float` | `textureParams.width/swidth/twidth` | the caller's footprint only; carried for FR-003 and the substitute log |
| `fill` | `float` | `textureParams.fill` | out-of-range samples, substitutes, the no-system result |
| `samples` | `float` | `textureParams.samples` | 2D `lookup4`, and through it spherical/cylindrical environments |
| `traceSamples` | `float` | `traceParams.samples` | shadow, deep shadow, cubic environment |
| `bias` | `float` | `traceParams.bias` | shadow, deep shadow |
| `stream` | `TextureSampleStream` | `{sampling.backend, drawCursor, drawCursorDim}` of the context | every stochastic draw (`drawNext2`/`drawNext4`) |

`samples` and `traceSamples` stay `float`, as in the scratch, so `1.0f - 1.0f / samples` and `(int)samples` are
computed from the same bits. `TextureSampleStream` = `{Backend backend; SubstreamKey key; uint32_t dim;}` with
inline `next()`, `next2(float[2])`, `next4(float[4])`, each `get1D(backend, key, dim++, 0)`.

Per builtin (the interpreter's and the JIT's sources are the same scratch, read at the same moment as today):

| Builtin | Handle kind | Coordinates handed to the system | Notes |
|---|---|---|---|
| `texture()` interpreter | `Texture` | four `(s, t)` corners from the derivative footprint, or the eight given | `texture(…)` method (today `lookup4`) |
| `texture()` JIT | `Texture` | phases 6–8: one `(s, t)` through `texturePoint(…)` (today's point `lookup`); from phase 9: the interpreter's request (FR-022) | |
| `environment()` interpreter | `Environment` | `D0..D3` from the footprint | not called for `"raytrace"` or the shader's own name |
| `environment()` JIT | `Environment` | phases 6–8: `D, D, D, D`; from phase 9: the interpreter's footprint (FR-022) | |
| `shadow()` interpreter / JIT | `Environment` | `P0..P3` / `P, P, P, P` | each backend reduces the three channels to its float result as it does today |
| occlusion/indirectdiffuse environment (hider) | `Environment` (handle stored in `occlusionParams.environment`) | `ray.dir` ×4 | built by `lookupEnvironment()`, `traceSamples` = 1 as the hider sets it |
| `textureinfo()` | `Info` | — | `describe()` |

## 5. Dimensions consumed per lookup (FR-005, test T-K1)

| Lookup | Dimensions | Condition |
|---|---|---|
| 2D baked (`CMadeTexture::lookup4`) | 2 × `samples` | — |
| 2D unbaked from PNG, EXR or RGBE (synthesized pyramid, `CMadeTexture::lookup4`) | 2 × `samples` | blur only picks the level |
| 2D regular, a plain TIFF (`CRegularTexture::lookup4`) | 2 × `samples`, + 2 × `samples` | the second term when `blur > 0` |
| 2D point (`lookup`), `lookupz`, substitutes | 0 | — |
| shadow (`CShadow`), deep shadow (`CDeepShadow`) | 4 × `traceSamples` | — |
| cubic environment | 2 × `traceSamples` | — |
| spherical, cylindrical environment | its side's `lookup4` | the side draws from the same `TextureOpt::stream` |

## 6. `TexturePerthread`

| Field | Today |
|---|---|
| `thread` (`int`, `0 <= thread < numThreads`) | `context->thread` |
| `refNumber` (`int *`, one allocation per thread) | `CRenderer::textureRefNumber[thread]` |
| `usedMemory`, `maxMemory` (`int`) | `CRenderer::textureUsedMemory[thread]`, `textureMaxMemory[thread]` |
| `scratch` (`CMemPage *`, `memoryInit`/`memoryTini`) | `context->threadMemory` (borrowed today) |

Obtained with `CTextureSystem::perthread(thread)`; the system allocates `numThreads` of them at construction and
frees them at destruction. Every block's per-thread data (`CTexBlockThreadData[numThreads]`) is sized from the
same `numThreads`.

## 7. `CTextureCache` (Texturing-private)

The tile cache that `texture.cpp` keeps in `CRenderer` statics today, unchanged in behaviour (#19 out of scope):
the doubly linked list of used blocks, the block-registry and eviction mutex, the `TexturePerthread` array, the
LRU-by-reference-number eviction (`textureMemFlush`: evict until a thread's usage is at most half its budget),
block registration and unregistration. Every texture layer and deep-shadow tile gets a `CTextureCache *` at
construction, from the system that loads it. The process-wide pyramid-synthesis lock stays outside it
(research R13).

## 8. `TextureInfo`

`describe(handle, info)` fills, from the `CTextureInfoBase` behind the handle: `resolution[2]`
(`getResolution`), `type` (`getTextureType`, static string), `numChannels`, `hasViewMatrix` + `viewMatrix[16]`
(`getViewMatrix`), `hasProjectionMatrix` + `projectionMatrix[16]` (`getProjectionMatrix`). The arrays are zeroed
first, as both `textureinfo()` implementations zero their output buffer today, so a query that today wrote
nothing still reads zeros.

## 9. `CPointHierarchy::Options`

`{ float (*maxSolidAngle)(const CShadingContext *context); }`, given at construction by the core
(`rendererFiles.cpp`, the only constructor call). `lookup()` reads `maxsolidangle` through it from the context it
already receives (research R12). `CShadingContext` stays an opaque forward declaration in Texturing.

## 10. Frame lifecycle

```text
RiBegin ──▶ services registered (session)                 textureSystem() == nullptr
RiWorldBegin ─▶ beginFrame: … numThreads final ─▶ resolver + createTextureSystem(settings)
             ─▶ CRenderer::textureSystem = system           textureSystem() == system
             ─▶ contexts created (they fetch Perthread blocks per builtin call)
render ───────▶ handles created on first use, lookups
RiWorldEnd ──▶ endFrame: contexts deleted ─▶ … endDisplays ─▶ frameFiles->destroy (GI/3D kinds)
             ─▶ delete system (family cache, then tile cache, then Perthread blocks); textureSystem = nullptr
RiEnd ───────▶ services deregistered
```

## 11. With no texture system (FR-012, maintainer decision 2)

`textureCalls` handles it; the system is never called.

| Builtin | Result | Warning (once per kind per process, `CODE_NOTSTARTED`) |
|---|---|---|
| `texture()` | `fill` in each channel | `texture` |
| `environment()` (not `"raytrace"`) | `fill` in each channel | `environment` |
| `shadow()` | `fill` | `shadow` |
| `textureinfo()` | 0, nothing written | `textureinfo` |
| `texture3d()`, `bake3d()` (no services) | 0, nothing written | `texture3d`, `bake3d` |
| hider environment lookup | `fill` | `environment` |
