# Contract: the photon map (Texturing store, Shading access, Core bridge)

**Requirements**: FR-002, FR-004, FR-005, FR-009, FR-010, FR-017, FR-019, FR-028, FR-029. **Research**: R5, R6, R10,
R12.

## Files

| File | Domain | Status |
|---|---|---|
| `src/ri/texture/photonStore.{h,cpp}` | Texturing | new (the storage part of `src/ri/hiders/photonMap.{h,cpp}`) |
| `src/libshader/shading/photonMapAccess.h` | Shading | new, header-only |
| `src/ri/render/photonMap.h` | Core | new, header-only (the name `CPhotonMap` and `photonMap.h` move here) |
| `src/ri/hiders/photonMap.{h,cpp}` | Hiders | deleted |
| `src/ri/hiders/photon.{h,cpp}` | Hiders | edited: the store path through `photonMapOf`, `balanceList`'s type, `tracePhoton`'s key argument, `writePhotons(CRenderer::world)` |
| `src/ri/core/map.h` | Infra | edited: the `permuted` hook (`gi-keys.md`) |
| `rendererFiles.cpp`, `renderer.cpp`, `rendererContext.cpp`, `rendererStatics.cpp`, `rendererServicesImpl.h`, `rendererContext.h`, `renderer.h` | Core | include `render/photonMap.h` (the bare `photonMap.h` include resolves to it); otherwise unchanged |
| `src/ri/dataviewer/dataLoad.cpp` | Consumer | include path only; still `new CPhotonMap(fileName, in)` |
| `src/ri/state/stateHandles.h` | Graphics State | comment only (`CMapRefTarget` is a base of `CPhotonMapRefTarget`) |
| `src/libshader/shading/photonLookup.h` | Shading | comment only (implemented by `CPhotonMap`, now in Core) |

## `photonStore.h` (Texturing)

```cpp
class CPhoton : public CMapItem { public: vector C; unsigned char theta, phi; };   // unchanged, 44 bytes

class CPhotonStore : public CMap<CPhoton>, public CFileResource {
    public:
        // fromWorld/toWorld: the creator's current world matrices (were CRenderer::fromWorld/toWorld, FR-005).
        CPhotonStore(const char *name, FILE *in, const float *fromWorld, const float *toWorld);
        ~CPhotonStore() override;
        void reset();
        void write(const float *fromWorld, const float *toWorld);   // writes these matrices (were CRenderer's)
        void lookup(float *C, const float *P, int maxFound);         // Shading's photonmap() estimate
        void lookup(float *C, const float *P, const float *N, int maxFound); // the hiders' estimate
        void balance() override;
        void store(const float *P, const float *N, const float *I, const float *C);            // key 0
        void store(const float *P, const float *N, const float *I, const float *C, uint64_t key);
        void bound(float *bmin, float *bmax) const;
        const char *typeName() const { return "Photon Map"; }
        uint64_t photonKey(int i) const;                             // gi-keys.md
        // public as today: modifying, from, to, maxPower, searchRadius, root/maxDepth (PHOTON_LOOKUP_CACHE), mutex
    protected:
        void permuted(CPhoton *const *sorted, const CPhoton *oldItems, int count) override; // gi-keys.md
};
```

The bodies are `photonMap.cpp:65-552` with two changes:
- `CRenderer::toWorld/fromWorld` become the constructor's and `write`'s arguments.
- The key side array is added (Phase 7).

The direction tables move here. The `object.h`/`raytracer.h` includes are dropped.

## `photonMapAccess.h` (Shading)

```cpp
#include "photonLookup.h"
#include "stateHandles.h"         // CMapRefTarget, MapRef (Shading -> Graphics State)
class CXform;                      // Graphics State

namespace openrender::shading {
    // The hiders' estimate: irradiance at P with normal N from the maxFound nearest photons (C: 3 floats). The body is
    // CPhotonStore::lookup(C, P, N, maxFound). Version: kRendererServicesVersion (introduced in 2.2.0).
    class IPhotonEstimator {
        public:
            virtual ~IPhotonEstimator() = default;
            virtual void estimate(float *C, const float *P, const float *N, int maxFound) = 0;
    };
    // What the photon hider writes. Not thread-safe beyond what CPhotonMap was (storePhoton locks; the modifying flag
    // is the same unguarded int as before).
    class IPhotonStore {
        public:
            virtual ~IPhotonStore() = default;
            virtual void storePhoton(const float *P, const float *N, const float *I, const float *C, uint64_t key) = 0;
            virtual void resetPhotons() = 0;
            virtual void balancePhotons() = 0;
            virtual int isModifying() const = 0;
            virtual void setModifying(int) = 0;
            virtual void writePhotons(const CXform *world) = 0;   // world's from/to are the matrices written
    };
    // What a Graphics State map slot holds (spec 025): the photon map, seen by Shading and the hiders.
    class CPhotonMapRefTarget : public CMapRefTarget, public IPhotonLookup, public IPhotonEstimator, public IPhotonStore {
        protected:
            CPhotonMapRefTarget() = default;
    };
    // The only conversion of a map handle (a map slot only ever holds a CPhotonMap, which derives from this class).
    inline CPhotonMapRefTarget *photonMapOf(const MapRef &r) { return static_cast<CPhotonMapRefTarget *>(r.get()); }
}
```

## `render/photonMap.h` (Core)

```cpp
#include "photonStore.h"          // Core -> Texturing
#include "photonMapAccess.h"      // Core -> Shading
#include "photonMapView.h"        // Core -> Consumer (the data viewer's adapter; viewer-adapters.md)
#include "renderer.h"             // CRenderer::fromWorld/toWorld
#include "xform.h"

class CPhotonMap : public CPhotonStore, public openrender::shading::CPhotonMapRefTarget, public CPhotonMapView {
    public:
        CPhotonMap(const char *name, FILE *in)
            : CPhotonStore(name, in, CRenderer::fromWorld, CRenderer::toWorld), CPhotonMapView(this) { attach(); }
        // IPhotonLookup / IPhotonEstimator / IPhotonStore: inline forwards to CPhotonStore
        void lookup(float *C, const float *P, int maxFound) override { CPhotonStore::lookup(C, P, maxFound); }
        void estimate(float *C, const float *P, const float *N, int n) override { CPhotonStore::lookup(C, P, N, n); }
        void storePhoton(...) override; void resetPhotons() override; void balancePhotons() override;
        int isModifying() const override { return modifying; } void setModifying(int m) override { modifying = m; }
        void writePhotons(const CXform *world) override { write(world); }
        // The 1-argument write: world's matrices, or CRenderer's when world is null (test_data_world_init: write(NULL))
        void write(const CXform *world) {
            CPhotonStore::write(world ? world->from : CRenderer::fromWorld, world ? world->to : CRenderer::toWorld);
        }
        // CDataView members that are also storage members: one final overrider each
        void bound(float *bmin, float *bmax) override { CPhotonStore::bound(bmin, bmax); }
        const char *typeName() const override { return CPhotonStore::typeName(); }
        // balance(): CPhotonStore::balance (virtual through CMap) stays the final overrider
};
```

- **Header-only.** There is no `render/photonMap.cpp`, because `libRIBVector` (the data viewer) constructs the map
  and the residual list may not grow (research R5).
- **Bases.** One `CRefCounter` base, through `CMapRefTarget`, so `map->refCount` and the `attach()`/`detach()` of the
  #64 tests stay unambiguous. Polymorphic, so `dynamic_cast<void *>` works.
- **`photonMapOf` and its callers.** The old Hiders `photonMapOf` (returning `CPhotonMap *`) is deleted. Its callers
  (`photon.cpp`, the sampler) use Shading's. `rendererContext.cpp` binds slots with `MapRef::retain(getPhotonMap(..))`
  as today, through the implicit upcast.

## Hiders call sites

| Where | Today | After |
|---|---|---|
| `photon.cpp:34` | `#include "photonMap.h"` | `#include "photonMapAccess.h"` |
| `photon.h:39,77` | `class CPhotonMap;`, `CArray<CPhotonMap *> balanceList` | `CArray<openrender::shading::CPhotonMapRefTarget *> balanceList` |
| `photon.cpp:95-103` | `cMap->write(CRenderer::world)` | `cMap->writePhotons(CRenderer::world)` (the matrices `CRenderer::fromWorld/toWorld` are copied from, `renderer.cpp:300-303`) |
| `photon.cpp:630-662` | `CPhotonMap *globalMap = photonMapOf(...)`; `->modifying`, `->reset()`, `->store(Pl, Nl, ray.dir, Cl)` | `CPhotonMapRefTarget *`; `isModifying()`/`setModifying(TRUE)`, `resetPhotons()`, `storePhoton(Pl, Nl, ray.dir, Cl, photonKey(pointKey.full, bounce))`, in the same order and under the same conditions |
| `irradianceDispatch.cpp:477` (moves to the sampler) | `globalMap->lookup(C, P, N, est)` | `photonMapOf(...)->estimate(C, P, N, est)` |
| `radiance.cpp:315, 1119` | the same | the file is deleted with `radiance.h` and `harmonics.h`: dead, compiled by no target (OQ-4, approved 2026-11-07) |

## Invariants

1. `.pm` files written at `-t:1` equal master's in every defined byte (T-F compares them field by field: item 0, the
   `CPhoton` padding and an empty map's dummy direction are never assigned, OQ-8). Maps written before this spec read
   back unchanged (FR-019).
2. `getPhotonMap`, `framePhotonMaps`, the release in `endFrame()` and the empty map for a missing file are unchanged
   (FR-010). The guards are `photonmap_FrameCacheRelease`, `photonmap_TwoFrameSession` and
   `photonmap_ObjectDefinitionRelease`.
3. `IPhotonLookup` is unchanged. Shading's `photonmap()` builtins (`giFunctions.h:283, 330`, `shading.cpp:4107`) are
   untouched, so the parity of #145 and #97 is as it is.
4. #104: `getPhotonMap`, `getCache` and `getTexture3d` still key `frameFiles` by name only. One name used for two GI
   kinds still answers the wrong class (unchanged, not fixed).
