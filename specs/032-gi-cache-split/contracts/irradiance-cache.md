# Contract: the irradiance cache (store, shared lookup, sampler)

**Requirements**: FR-001, FR-002, FR-004 to FR-006, FR-008, FR-027, FR-030. **Research**: R1 to R4, R11.

## Files

| File | Domain | Status |
|---|---|---|
| `src/ri/texture/irradianceStore.h` | Texturing | new, header-only |
| `src/ri/texture/irradianceCache.{h,cpp}` | Texturing | new (moved from `src/ri/hiders/irradiance.{h,cpp}`) |
| `src/libshader/shading/irradianceSampler.h` | Shading | new, header-only |
| `src/libshader/shading/irradianceCalls.{h,cpp}` | Shading | new |
| `src/ri/hiders/irradianceSampler.cpp` | Hiders | new (the body of `irradianceDispatch.cpp:68-587`) |
| `src/ri/hiders/hiderIrradianceSampler.h` | Hiders | new. Deliberately not `irradianceSampler.h`: Shading owns that basename, and a same-named Hiders header would include itself (a quoted include searches the including file's directory first) |
| `src/ri/hiders/irradiance.{h,cpp}`, `src/ri/hiders/irradianceDispatch.cpp`, `src/ri/vector/irradianceDispatchStub.cpp` | Hiders, Core | deleted |
| `giFunctions.h` (`IDEXPR`), `shading.cpp` (the JIT occlusion/indirectdiffuse body), `shading.h` | Shading | edited |
| `hider.cpp` | Hiders | edited (installs the sampler) |
| `rendererFiles.cpp`, `rendererShadingServices.cpp` (or `renderer.cpp`'s `beginRenderer`/`endRenderer`), `remoteChannel.{h,cpp}`, `rendererContext.cpp`, `renderer.cpp`, `rendererStatics.cpp` | Core | edited (include path, constructor arguments, default sampler) |

## `irradianceStore.h` (Texturing)

```cpp
const unsigned int CACHE_SAMPLE = 1, CACHE_READ = 2, CACHE_WRITE = 4, CACHE_RDONLY = 8; // moved, values unchanged

class CIrradianceRecord {               // the fields of today's CCacheSample, in order, without next
    public:
        vector P, N, irradiance; float coverage; vector envdir;
        float gP[7 * 3], gR[7 * 3];
        float dP;
        uint64_t key;                    // spec 040's key (gi-keys.md); 0 when read from a file
};

using IrradianceDrawFn = float (*)(void *drawState); // the next value of the caller's sample stream

class CIrradianceStore : public CTexture3d {
    public:
        using CTexture3d::CTexture3d;
        // Writes Pc/Nc (cache space). Calls draw(drawState) once for every record that passes the front test
        // (irradiance.cpp:365-368 today), at the point irradianceSampleAccept() is called today, in traversal
        // order. Hit: writes C[0..6] and returns true. Miss: returns false and leaves C alone.
        virtual bool lookupCached(float *C, const float *P, const float *N, float maxError,
                                  IrradianceDrawFn draw, void *drawState, float *Pc, float *Nc) = 0;
        // Under the cache's mutex: allocate, copy, clamp() against the tree, place by dP / (0.4f / maxError), link.
        virtual void insertRecord(const CIrradianceRecord &record, float maxError) = 0;
        virtual bool samplesOnMiss() const = 0;  // flags & CACHE_SAMPLE
        const float *cacheFrom() const { return from; }
        const float *cacheTo() const { return to; }
};

// getCache() answers only irradiance caches (rendererServices.h): Core's CRenderer::getCache dynamic_casts the entry it finds
// in frameFiles and answers nullptr for another kind (#104), so this cast never sees a wrong object. The only conversion allowed.
inline CIrradianceStore *irradianceStoreOf(CTexture3d *t) { return static_cast<CIrradianceStore *>(t); }
```

R10 adds the `key` field (Phase 7). Until then the record ends at `dP`.

## `CIrradianceCache` (Texturing)

What changes from `hiders/irradiance.{h,cpp}`:

- **Bases and includes.**
  - It derives from `CIrradianceStore`.
  - It includes only `texture3d.h`, `irradianceStore.h` and Infra headers. It loses `options.h`, `shader.h`,
    `depository.h`, `debug.h`, `photonMap.h`, `pointHierarchy.h`, `renderer.h`, `shaderPl.h`, `shading.h`,
    `surface.h` and `texture.h`.
- **Constructor.** `CIrradianceCache(name, flags, in, from, to, tondc, worldBmin, worldBmax)`:
  - `transformBound(bmin, bmax, to, worldBmin, worldBmax)` replaces the read of `CRenderer::worldBmin/worldBmax`.
  - Callers: `rendererFiles.cpp` (two sites) passes `CRenderer::worldBmin/worldBmax`; `dataLoad.cpp` passes the same
    statics, which it seeds.
- **`lookupCached`.** It is `irradiance.cpp:322-460` up to the miss decision, with two substitutions:
  - `scratch->occlusionParams.maxError` becomes `maxError`;
  - `irradianceSampleAccept(w, smallSampleWeight, context)` becomes `w > draw(drawState) * smallSampleWeight`.

  The traversal, the arithmetic and its order are unchanged. `smallSampleWeight` stays internal
  (`(flags & CACHE_SAMPLE) ? 0.1f : 0.0f`).
- **`insertRecord`** is `irradianceDispatch.cpp:592-666` without the gradient and radius computations, which have
  moved to the sampler:
  1. `osLock(mutex)`.
  2. Allocate a `CCacheSample` from `memory` and copy the record into it.
  3. `clamp(cSample)`.
  4. `rMean = cSample->dP / (0.4f / maxError)`.
  5. Descend and create nodes as today, then link and update `maxDepth`.
  6. `osUnlock(mutex)`.
- **`lookup(float *, P, dPdu, dPdv, N, CShadingContext *)`** is gone. `lookupPointBased` asserts, as the 4-argument
  `lookup`/`store` do.
- **`draw`, `keyDown`, `drawDiscs`** move to the viewer (`viewer-adapters.md`). **`friend CRemoteICacheChannel`**
  goes (`remote-channels.md`).
- **`sample()`** is gone. `irradianceSampleAccept` is gone everywhere.

## `irradianceSampler.h` (Shading)

```cpp
namespace openrender::shading {
    struct IrradianceSample { float C[7]; bool keep; float gP[7 * 3], gR[7 * 3]; float dP; };
    // Computes a new irradiance sample at P (cache space) for the shading call in progress on ctx: the hemisphere,
    // the probe rays (libshader::traceRay, so the context's tracer), environment lookups and photon-map reads.
    // `cacheFrom` maps cache space to the tracer's space. Draws from ctx's bound cursor exactly as
    // CIrradianceCache::sample() did. `keep` is maxError != 0 && coverage < 1 - C_EPSILON; when keep, gP, gR and dP
    // (rMean * 0.5 clamped to db * maxPixelDist) are filled.
    // Version: kRendererServicesVersion (introduced in 2.2.0). Thread-safety: stateless; any thread, its own ctx.
    class IIrradianceSampler {
        public:
            virtual ~IIrradianceSampler() = default;
            virtual void sample(CShadingContext &ctx, const float *cacheFrom, const float *P, const float *dPdu,
                                const float *dPdv, const float *N, IrradianceSample &out) = 0;
    };
}
```

`CShadingContext` (`shading.h`) gains:
- `void setIrradianceSampler(IIrradianceSampler *)` and `IIrradianceSampler *irradianceSampler() const`, beside
  `setTracer`;
- `static void setDefaultIrradianceSampler(IIrradianceSampler *)` and `static IIrradianceSampler
  *defaultIrradianceSampler()`, beside `setDefaultServices`;
- `const openrender::sampling::SubstreamKey &boundDraws() const`, which returns `drawCursor`.

All of them are inline or defined in `shading.cpp`.

## `irradianceCalls.h` (Shading)

```cpp
namespace libshader {
    // occlusion()/indirectdiffuse()'s cache step, shared by the interpreter (IDEXPR) and the JIT. Reads
    // scratch.occlusionParams (pointbased, pointHierarchy, maxError, maxSolidAngle) as CIrradianceCache::lookup()
    // did. Must be called after bindDraws() for the lane.
    void irradianceLookup(CShadingContext &ctx, CTexture3d *cache, float *C, const float *P, const float *dPdu,
                          const float *dPdv, const float *N);
}
```

Body: research R2, steps 1 to 4, preceded by step 0: a null `cache` (Core's `getCache` answers `nullptr` for a name registered as another 3D kind, see below) writes the no-joy value `{0, 0, 0, 1, 0, 0, 0}` and returns, drawing nothing. The sampler is resolved in this order:
1. `ctx.irradianceSampler()`;
2. `CShadingContext::defaultIrradianceSampler()`;
3. none: `warning(CODE_NOTSTARTED, ...)` once per context, then the unsampled miss.

The record key is `irradianceRecordKey(cursor)`, where `cursor` is a copy of `ctx.boundDraws()` taken at the **top**
of `irradianceLookup`, before the store lookup and the sampler run (`gi-keys.md`). The key then cannot depend on how
many records the lookup visited or on what the sampler's nested work did to the context.

Call sites (the text of each call is the only change):
- `giFunctions.h` `IDEXPR`: `cache->lookup(C, op1, dPdu, dPdv, op2, this);` becomes
  `libshader::irradianceLookup(*this, cache, C, op1, dPdu, dPdv, op2);`.
- `shading.cpp:3531`: `cache->lookup(C, JIT_IDX(P, sP, i), PduScaled, PdvScaled, JIT_IDX(N, sN, i), this);` becomes
  `libshader::irradianceLookup(*this, cache, C, JIT_IDX(P, sP, i), PduScaled, PdvScaled, JIT_IDX(N, sN, i));`.

`COcclusionLookup::map` stays `CTexture3d *`.

## `CIrradianceSampler` (Hiders)

- **`hiders/hiderIrradianceSampler.h`**: includes Shading's `irradianceSampler.h` (the bare name resolves to `src/libshader/shading/`, because no Hiders file has it); `class CIrradianceSampler final : public openrender::shading::IIrradianceSampler`,
  plus `openrender::shading::IIrradianceSampler &irradianceSamplerInstance();` (a function-local static).
- **`hiders/irradianceSampler.cpp`**:
  - `horizonCutoff`, `CHemisphereSample`, `posGradient` and `rotGradient` are moved verbatim.
  - `sample()` is `irradianceDispatch.cpp:252-587` with these substitutions: `context->` becomes `ctx.`, the cache's
    `from` becomes `cacheFrom`, and the photon map read becomes `photonMapOf(attributes->globalMap)->estimate(C, P,
    N, attributes->photonEstimator)` (`photon-map.md`).
  - Then, when `keep`: `posGradient(out.gP, ...)`, `rotGradient(out.gR, ...)`, `rMean *= 0.5f`, the `maxPixelDist`
    clamp, `out.dP = rMean`. These are the same expressions in the same order, now outside the lock.
- **Includes**: `hiderIrradianceSampler.h`, `shading.h`, `shaderPl.h`, `traceCalls.h`, `photonMapAccess.h`, `object.h`
  (Geometry, for `ray.object->attributes`), Infra. No `src/ri/texture` header and no `renderer.h`. Phase 4 runs an
  `nm -u` check of its object for Texturing symbols.
- **Installation**: `CHider::CHider` calls `context.setIrradianceSampler(&irradianceSamplerInstance())`.
  `CRenderer::beginRenderer` calls `CShadingContext::setDefaultIrradianceSampler(&irradianceSamplerInstance())`, and
  `endRenderer` resets it to `nullptr`, in the same place as `setDefaultServices` (`rendererShadingServices.cpp:65,81`). `hider.cpp` and `rendererShadingServices.cpp` include `hiderIrradianceSampler.h`.

## Invariants (checked by the GI test set and T-D)

1. The draw count per lookup is 1 per record that passes the front test, on the store path only. The order is the
   traversal order, then the sampler's draws. Unchanged.
2. Insert order, tree shape and `maxDepth` are unchanged at `-t:1`. Lookups read without a lock; inserts and `clamp()`
   hold the mutex.
3. `C`, the envdir transform, the "no joy" value and the NaN assert are unchanged.
4. `libshader_shading` imports no new symbol. The calls into the store and the hierarchy are virtual; `irradianceStoreOf`
   is a `static_cast`.

## Test seam: `CIrradianceCache` and `insertRecord` stay non-final

T-E's `GiKeys_IrradianceRecords` (maintainer answer at PAUSE 2a, 2026-11-09) reads `(key, record)` pairs through a test-only
subclass of `CIrradianceCache` that overrides `insertRecord()` to copy each record before forwarding, and hands it to Shading
through a services wrapper (`CContractForwarder` with `getCache()` replaced). So `CIrradianceCache` must not be `final` and
`CIrradianceStore::insertRecord` must stay virtual and overridable, and its constructor must stay inheritable
(`using CIrradianceCache::CIrradianceCache`). Nothing in `src/` is added for the test.
