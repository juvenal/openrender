# Contract: the texture system (Texturing)

New headers in `src/ri/texture/`. They include only infrastructure (`src/common`): Texturing depends on no other
domain (constitution Principle VIII). Shading and the renderer core include them; the hiders reach the types only
through Shading's `shading.h` (no new Hiders → Texturing edge). Names follow OSL's `TextureSystem` vocabulary with
the house prefixes: `TextureSystem` → `CTextureSystem`, `Perthread` → `TexturePerthread`, `TextureHandle`,
`TextureOpt`. Spec 026 adopts this interface as the texture part of its services contract.

## `textureResolver.h`

```cpp
// Finds texture-related files for Texturing, which may not call the renderer core (FR-007).
class CTextureFileResolver {
    public:
        virtual ~CTextureFileResolver() = default;

        // Writes the full path of `name` into `result` (OS_MAX_PATH_LENGTH bytes) and returns TRUE
        // when the file exists; FALSE otherwise. Must be callable from several threads at once.
        virtual int locate(char *result, const char *name) const = 0;
};
```

- The renderer core implements it as `CSearchPathResolver` (`src/ri/render/searchPathResolver.h` (new) and `rendererFileSearch.cpp`, compiled
  into `ribRender` and `ribVector` like `locateFile` itself) over a `TSearchpath *`, calling
  `CRenderer::locateFile(result, name, path)`: same search order, network mapping, temporary-directory fallback
  and `CODE_RESOLUTION` messages as today.
- `texmake.h` and `brickmap.h` take `const CTextureFileResolver *resolver` where they take `TSearchpath *` today
  (`makeTexture`, `makeSideEnvironment`, `makeCubicEnvironment`, `makeSphericalEnvironment`,
  `makeCylindericalEnvironment`, `makeBrickMap`, and `makeBrickMap`'s friend declaration in `pointCloud.h`).
  Parameter order is otherwise unchanged.

## `textureSystem.h`

```cpp
#include <cstdint>
#include "common/algebra.h"      // matrix
#include "common/global.h"       // TRUE/FALSE
#include "common/os.h"           // TMutex, osLock/osUnlock
#include "common/riVocabulary.h" // RtFilterFunc
#include "common/sampling.h"     // Backend, SubstreamKey, get1D

class CTextureFileResolver;

struct TextureHandle;    // opaque: never defined outside Texturing
struct TexturePerthread; // opaque: never defined outside Texturing

enum class ETextureKind { Texture, Environment, Info };

// A position in a counter-based sub-stream (spec 039). Each draw takes the next dimension.
struct TextureSampleStream {
        openrender::sampling::Backend backend;
        openrender::sampling::SubstreamKey key;
        uint32_t dim;

        float next() { return openrender::sampling::get1D(backend, key, dim++, 0); }
        void next2(float *r) { r[0] = next(); r[1] = next(); }
        void next4(float *r) { for (int k = 0; k < 4; k++) r[k] = next(); }
};

// The per-call options of a lookup (FR-003). Built by the caller, passed by reference; the
// lookup advances `stream`, and the caller reads the advanced cursor back (FR-005).
struct TextureOpt {
        RtFilterFunc filter;
        float blur;
        float width, swidth, twidth; // the caller's footprint; no lookup reads them
        float fill;
        float samples;      // 2D filter samples ("samples" of texture())
        float traceSamples; // "samples" of environment()/shadow() (CTraceLookup's block)
        float bias;         // "bias" of shadow()
        TextureSampleStream stream;
};

struct TextureInfo {
        float resolution[2];
        const char *type;
        int numChannels;
        int hasViewMatrix;
        float viewMatrix[16];
        int hasProjectionMatrix;
        float projectionMatrix[16];
};

struct CTextureSystemSettings {
        int numThreads;
        int maxMemory;                        // bytes, split evenly per thread
        const CTextureFileResolver *resolver; // not owned, outlives the system
        bool diskCacheEnabled;
        const char *diskCacheDirectory;       // copied; nullptr = the resolved default
        matrix toWorld;                       // copied; for shadow and deep-shadow maps
        TMutex *loadMutex;                    // not owned; nullptr = the system's own
};

class CTextureSystem {
    public:
        virtual ~CTextureSystem() = default;

        // Handles (FR-002, FR-016). Thread-safe: one name is located, loaded and reported once.
        virtual TextureHandle *handle(const char *name, ETextureKind kind) = 0;

        // Per-thread data (FR-004), 0 <= thread < numThreads.
        virtual TexturePerthread *perthread(int thread) = 0;

        // Lookups. `handle` is never null here (the caller handles the no-system case);
        // `result` receives 3 floats.
        virtual void texture(TextureHandle *handle, TextureOpt &opt, TexturePerthread *pt,
                             const float *s4, const float *t4, float *result) = 0;   // filtered, 4 corners
        virtual void texturePoint(TextureHandle *handle, TextureOpt &opt, TexturePerthread *pt,
                                  float s, float t, float *result) = 0;              // unfiltered point
        virtual void environment(TextureHandle *handle, TextureOpt &opt, TexturePerthread *pt,
                                 const float *D0, const float *D1, const float *D2, const float *D3,
                                 float *result) = 0;                                 // environment, shadow, deep shadow
        virtual void describe(TextureHandle *handle, TextureInfo &out) = 0;          // textureinfo()
};

// The system the renderer core constructs once per frame. nullptr (and CODE_BUG) on invalid settings.
CTextureSystem *createTextureSystem(const CTextureSystemSettings &settings);
```

## Semantics

- **Handles**: `handle(name, kind)` for one name returns the same pointer for the system's lifetime. The family
  cache is keyed by name only, as `frameFiles` is today, so the first kind loaded owns the name (data-model §3).
  `Texture` and `Environment` never return `nullptr`: a missing or unreadable file yields a substitute (the
  existing `CDummyTexture`/`CDummyEnvironment`) after one `CODE_NOFILE` error with today's text. `Info` returns
  `nullptr` for a missing file and caches nothing (today's `getTextureInfo`).
- **Locking**: `handle()` holds `loadMutex` (or the system's own mutex) across find → load → insert. **Core-lock coupling**: the core passes `&CRenderer::shaderMutex`, a plain non-recursive mutex, so no code that already holds it may call `handle()` (a debug assertion catches a re-entry); the services getters that hold it (`getCache`, `getTexture3d`, `getPhotonMap`) never reach a texture handle. The block
  registry and eviction take the tile cache's own mutex, and the per-block mutexes are unchanged. Unbaked-source
  synthesis takes the process-wide synthesis lock (research R13).
- **Lookups**: identical arithmetic, sample order and memory accounting to today's `lookup4`/`lookup`/`lookupz`
  and `CEnvironment::lookup`, with `TextureOpt` in place of the scratch reads (data-model §4) and
  `TexturePerthread` in place of `context->thread`, `context->threadMemory` and the `CRenderer` per-thread arrays.
  Dimensions consumed are data-model §5's.
- **No shading context**: no declaration in `textureSystem.h`, `textureResolver.h`, `texture.h` or `texture.cpp`
  names `CShadingContext`. The six-argument `CTexture3d::lookup` and its overrides keep it (FR-014, SC-005).
- **Null resolver and `Info` misses**: a `nullptr` resolver given to a `make*` function or a loader means "no search path": the name is used as given (a path, or relative to the working directory) when the file exists, and nothing else is searched (the core's `locateFile` also tries its temporary directory, which Texturing cannot know; `tests/preview/test_data_keys.cpp` passes a bare name and a null path and is found through the empty temporary path, i.e. the working directory, so the null branch keeps that). An `Info` request for a missing file returns `nullptr`, caches nothing (the next request locates again) and reports nothing.
- **Lifetime**: constructed in `CRenderer::beginFrame()` where `initTextures()` ran, destroyed in `endFrame()`
  where `shutdownTextures()` ran. Destroying it frees every handle, texture, block and per-thread block.
- **Not covered**: 3D textures, point clouds, brick maps, irradiance caches and photon maps (spec 032). Their
  channel operations go through the renderer services (contracts/renderer-services.md).

## Texturing classes after the change (internal, `texture.h` keeps its path)

`CTexture::lookupz/lookup/lookup4` and `CEnvironment::lookup` take `(…, TextureOpt &opt, TexturePerthread *pt)` in
place of `CShadingContext *context`. `texture.h` stops including `attributes.h`. The hiders keep including
`texture.h`, `texture3d.h` and `pointHierarchy.h` at the same paths (FR-014, FR-019).

`CPointHierarchy(const char *name, const float *from, const float *to, FILE *in, const Options &options)`, with
`struct Options { float (*maxSolidAngle)(const CShadingContext *context); };`. `pointHierarchy.cpp` stops
including `shading.h`; the six-argument `lookup` keeps its signature.

## What the tests rely on

- `TextureOnly_Standalone`, `TextureSystem_ConcurrentFirstLoad`, `TextureSystem_StreamCursor` construct a system
  with `createTextureSystem` and a test resolver, with no renderer, no frame and no `RiBegin`.
- `createTiffTileSource(filename, directory)` and `createSynthesizedTileSource(filename, level)` keep their
  signatures (FR-008); `TileSource_TiffInfo` and `TileSource_SynthesizedInfo` change at most in include paths.
