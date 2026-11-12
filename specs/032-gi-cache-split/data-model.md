# Data Model: GI Cache Split (spec 032)

In-memory structures only. No file format changes (FR-019). Each entity names its owning domain, because the
allowed-dependency table decides who may see it.

## Irradiance cache

### `CIrradianceRecord` (Texturing, `texture/irradianceStore.h`)

The value of one cache record. The fields and their order are today's `CCacheSample`, without `next`.

| Field | Type | Notes |
|---|---|---|
| `P`, `N` | `vector` | cache space |
| `irradiance` | `vector` | |
| `coverage` | `float` | |
| `envdir` | `vector` | cache space, normalized |
| `gP`, `gR` | `float[21]` each | translational and rotational gradients, 7 channels x 3 |
| `dP` | `float` | validity radius; `insertRecord` clamps it and `clamp()` lowers it later |
| `key` | `uint64_t` | **new** (R10). Offset 224 on LP64; 0 for a record read from a file |

Validation:
- `key` is never read by a rendering path (FR-018).
- `writeNode`/`readNode` write and read the fields above except `key`, field by field as today.

### `CCacheSample` (Texturing, `texture/irradianceCache.h`)

A standalone, standard-layout class (it does not derive from `CIrradianceRecord`: the approved loopback test takes `offsetof(next)`): the record's fields in the same order, then `CCacheSample *next`
(offset 224, `sizeof` 232 on LP64), with `get(CIrradianceRecord &)` and `set(const CIrradianceRecord &)` to copy between the two. After R10 the record gains `key` and `next` moves to 232 (`sizeof` 240); the
cache keeps `sizeof(CIrradianceRecord) == offsetof(CCacheSample, next)` by a `static_assert`. The remote channel sends these raw bytes (`next` masked by the loopback test).

### `CCacheNode` (Texturing)

Unchanged: `samples` (list head), `children[8]`, `center`, `side`.

### `CIrradianceStore` (Texturing, header-only abstract, derives `CTexture3d`)

| Member | Kind | Contract |
|---|---|---|
| `lookupCached(C, P, N, maxError, draw, drawState, Pc, Nc)` | pure virtual | Writes `Pc`/`Nc` in cache space. Calls `draw(drawState)` once per record that passes the front test, in traversal order. Hit: writes `C[0..6]` (envdir in the caller's space), returns `true`. Miss: returns `false`, `C` untouched |
| `insertRecord(const CIrradianceRecord &, float maxError)` | pure virtual | Under the mutex: allocates, copies, `clamp()`, places at the depth given by `dP / (0.4 / maxError)`, links, updates `maxDepth` |
| `samplesOnMiss()` | pure virtual | `flags & CACHE_SAMPLE` |
| `cacheFrom()`, `cacheTo()` | inline | the `CTexture3d` matrices |

`irradianceStoreOf(CTexture3d *)` is an inline `static_cast`.

### `CIrradianceCache` (Texturing, implements `CIrradianceStore`)

- **Construction.** `CIrradianceCache(name, flags, in, from, to, tondc, worldBmin, worldBmax)`. The world bound is
  now an argument (FR-005).
- **Lifecycle.** As today:
  - The file is read in the constructor when `CACHE_READ`.
  - A fresh root is built over the transformed world bound otherwise.
  - The file is written in the destructor when `CACHE_WRITE`.
- **Read interface** (R7): `forEachNode`, `forEachRecord`, `rootCenter`, `rootSide`, `setRoot`, `insertReceived`,
  `treeDepth`, plus `bound`, `typeName`, `numChannels`, `channelName`.

### `IrradianceSample` (Shading, `libshader/shading/irradianceSampler.h`)

The sampler's answer: `C[7]`, `bool keep`, `gP[21]`, `gR[21]`, `dP` (already halved and clamped to
`maxPixelDist * db`). It carries no Texturing type.

### `IIrradianceSampler` (Shading interface)

Implemented by `CIrradianceSampler` (Hiders, `hiders/hiderIrradianceSampler.h`). There is one stateless instance. The context's owning hider installs it
(`CHider`), and the core installs it as the process default at `RiBegin`.

## Photon map

### `CPhoton` (Texturing, `texture/photonStore.h`)

Unchanged: `CMapItem` (`P`, `N`, `flags`) + `C` + `theta`, `phi`. 44 bytes.

### `CPhotonStore` (Texturing; derives `CMap<CPhoton>` and `CFileResource`)

| State | Notes |
|---|---|
| `items`, `numItems`, `maxItems`, `bmin`, `bmax` | `CMap` |
| `keys` | **new**: a side array of `uint64_t`, index-aligned with `items`. It grows with `store`, is cleared by `reset`, is filled with 0 by `read`, and is permuted by `balance` through `CMap::permuted` |
| `root`, `maxDepth` | the lookup octree (`PHOTON_LOOKUP_CACHE`) |
| `from`, `to` | public. Set from the file's matrices times the creator's current world matrices |
| `maxPower`, `searchRadius` | public |
| `modifying` | public `int` (`test_data_world_init` writes it) |
| `mutex` | store and probe insert |

Operations:
- `store(P, N, I, C)` and `store(P, N, I, C, key)`;
- `balance()` (it adds the dummy photon when the map is empty);
- `reset()`, `write(fromWorld, toWorld)`;
- `lookup(C, P, maxFound)`, `lookup(C, P, N, maxFound)`;
- `bound`, `typeName`, `photonKey(i)`.

### `IPhotonEstimator`, `IPhotonStore`, `CPhotonMapRefTarget` (Shading, `libshader/shading/photonMapAccess.h`)

See `contracts/photon-map.md`. `CPhotonMapRefTarget` derives from `CMapRefTarget` (Graphics State),
`IPhotonLookup`, `IPhotonEstimator` and `IPhotonStore`. `photonMapOf(const MapRef &)` returns
`CPhotonMapRefTarget *`.

### `CPhotonMap` (Core, `render/photonMap.h`, header-only)

`CPhotonStore` + `CPhotonMapRefTarget` + `CPhotonMapView` (data viewer).

Lifetime is unchanged (#64):
- `getPhotonMap` constructs it and pushes it on `framePhotonMaps`.
- The constructor's `attach()` counts the cache's reference.
- `CAttributes` handles hold further references.
- `endFrame()` releases the cache's reference.
- A missing file gives an empty map.

## 3D family (Texturing)

### `CTexture3d`

- Loses `CDataView`.
- Keeps `CFileResource`, the channel table, `dataSize`, `from`, `to`, `toNDC` and `dPscale`.
- Gains public `defineChannels(const char *)`, `setTransform`, `setDPScale`, `channelSampleStart(i)` and
  `channelNumSamples(i)`.
- Its point-based virtual is `lookupPointBased(..., const CPointBasedLookup &)`, where `CPointBasedLookup` is
  `{ float maxSolidAngle; }`.

### `CPointCloud`

Loses its draw, key and viewer-state members and the remote friend. Gains `points()` and `pointData()`. The
`lookup`/`store` bodies are unchanged.

### `CBrickMap`

Loses its draw, key and viewer-state members and both `CBrickMapGeometry` lines. Gains
`visitVoxels(level, visitor)`, `maxLevel()` and `sideLength()`. Paging is unchanged and private.

### `CPointHierarchy`

Loses `Options` and its empty viewer members. Implements `lookupPointBased`; the traversal body is unchanged.

## Data viewer (Consumer)

| Adapter | Over | Static state (initial value as today) | Document class |
|---|---|---|---|
| `CPhotonMapView` | `const CPhotonStore *` | none | `CPhotonMap` (Core) derives from it |
| `CIrradianceCacheView` | `const CIrradianceCache *` | `drawDiscs` (TRUE) | `CIrradianceCacheDocument : CIrradianceCache, CIrradianceCacheView` |
| `CPointCloudView` | `const CPointCloud *` | `drawDiscs`, `drawChannel` | `CPointCloudDocument : CPointCloud, CPointCloudView` |
| `CBrickMapView` | `CBrickMap *` (loads bricks) | `detail`, `drawType`, `drawChannel` | `CBrickMapDocument : CBrickMap, CBrickMapView` |

The observable values are those of research R6's table.

## State transitions

- **Irradiance cache, per lookup** (unchanged in effect): point-based, or a store lookup that hits, or a miss that
  either samples and maybe inserts or returns the unsampled value. Inserts happen only on a sampled miss with
  `maxError != 0` and coverage below `1 - C_EPSILON`.
- **Photon map, per frame** (unchanged):
  - The first store of a frame in a map whose `isModifying()` is false sets it, resets the map and queues it on the
    photon hider's balance list.
  - The hider's destructor calls `writePhotons(CRenderer::world)` on each queued map, which balances, writes and
    clears `modifying`.
