# Contract: `IPhotonLookup` (Shading-owned, implemented by `CPhotonMap`)

**Requirements**: FR-012, FR-014, FR-015 (the spec's Q2, option A). **Research**: R5, R11.
**Files**: new `src/libshader/shading/photonLookup.h`; edited `rendererServices.h`, `shading.{h,cpp}` (the accessor
and `jitPhotonMap`), `giFunctions.h` (two declarations), `src/ri/hiders/photonMap.h`,
`src/ri/render/rendererServicesImpl.h`; the six test overrides of `getPhotonMap`.

## `photonLookup.h`

```cpp
namespace openrender::shading {
    // What Shading asks of a photon map (spec 029). Implemented by the photon map (CPhotonMap, Hiders), whose
    // storage, file handling and balancing stay in Hiders until spec 032. Returned by
    // CRendererServices::getPhotonMap(). Introduced in 3.0.0. Version: kRendererServicesVersion.
    // Thread-safety: as CPhotonMap::lookup today (concurrent lookups on a balanced map). Re-enters Shading: never.
    class IPhotonLookup {
        public:
            virtual ~IPhotonLookup() = default;   // inline: vtable and typeinfo are weak, wherever used
            // The irradiance estimate at P from the maxFound nearest photons, written to C (3 floats);
            // P in the map's space as the caller passes it today. The body is CPhotonMap::lookup(float *, const float *, int).
            virtual void lookup(float *C, const float *P, int maxFound) = 0;
    };
}
```

## Changes

| Where | Today | After |
|---|---|---|
| `rendererServices.h:316` | `virtual CPhotonMap *getPhotonMap(const char *name) = 0;` | `virtual openrender::shading::IPhotonLookup *getPhotonMap(const char *name) = 0;` (MAJOR) |
| `rendererServices.h:52`, `shading.h:60` | `class CPhotonMap;` | deleted |
| `shading.h:977`, `shading.cpp:567` | `CPhotonMap *rendererGetPhotonMap(const char *)` | returns `IPhotonLookup *` |
| `giFunctions.h:283, 330` | `CPhotonMap *map;` | `IPhotonLookup *map;` (the calls `:298`, `:344` unchanged in text) |
| `shading.cpp:4010` | `CPhotonMap *map = ...` | `IPhotonLookup *map = ...` (the call `:4023` unchanged in text) |
| `photonMap.h:75` | `class CPhotonMap : public CMap<CPhoton>, public CFileResource, public CDataView, public CMapRefTarget` | `..., public CMapRefTarget, public openrender::shading::IPhotonLookup` and `void lookup(float *, const float *, int) override;` |
| `rendererServicesImpl.h:67-72` | returns `CPhotonMap *` | returns `IPhotonLookup *` (the same object, upcast; the lock and the core call unchanged) |
| test doubles | `CPhotonMap *getPhotonMap(const char *) override` | `IPhotonLookup *getPhotonMap(const char *) override` (TEST-APPROVAL: `contract_services.h:242`, `contract_forwarder.h:92`, `recording_services.h:67, 163`, `table_services.h:61`, `recording_texture_system.h:234`) |

`photonMap.h` includes `photonLookup.h` (Hiders -> Shading, allowed). `execute.cpp` and `shading.cpp` stop
including `photonMap.h` (2 DB-005 lines) and stop referencing `CPhotonMap::lookup` (2 DB-001 lines).

## Behaviour

The same body runs on the same object with the same arguments; only the dispatch becomes virtual. A missing
map (`nullptr`) is handled by the callers exactly as today. `ribVector` (which compiles `photonMap.cpp` and
links no `libshader_shading`) needs no Shading symbol: the interface has no out-of-line member.

## Test

`ShadingContract_PhotonLookup{Interp,Jit}` (standalone, no `ribRender`): the services double returns a
recording `IPhotonLookup` for a named map; `photonmap("m", P)` under each backend calls `lookup` once per active
real point with the shader's `P` and the estimator `photonEstimator` (or the parameter), and returns the
double's value, replicated into the derivative tail as `expandVector` does. Red on the starting tree: build
failure (no `photonLookup.h`, `getPhotonMap`'s type).
