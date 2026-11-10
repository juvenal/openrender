# Research: GI Cache Split (spec 032)

Phase 0 of the plan. Every item is a decision with its rationale and the alternatives weighed. The spec's
Clarifications (Q1 to Q9, session 2026-11-07) are decisions taken by the maintainer; this file only places and shapes
what they leave open. Measurements are static reads of the starting tree (master `e83d005d`); Phase 1 re-measures
anything a build can confirm.

## M. Measurements at the plan stage

| What | Value | How |
|---|---|---|
| `DB-` lines in `tests/architecture/allowlist.txt` | 344 (DB-004 52, DB-006 30, DB-010 8, DB-011 164, DB-017 78, DB-025 12) | `grep -c '^DB-'`, `awk '{print $1}' \| uniq -c` |
| DB-010 lines | 8: `irradiance.cpp` (2 include, 3 link), `irradiance.h` (1 include), `irradianceDispatch.cpp` (2 include) | allowlist:100-107 |
| DB-025 lines | 12: `geometry/surface.cpp` (1, spec 031's), `hiders/irradiance.cpp` (1 include, 2 link), `hiders/photonMap.h` (1 include), `hiders/photonMap.cpp` (1 link), `texture/texture3d.h` (1 include), `texture/brickmap.cpp` (3 link), `texture/pointCloud.cpp` (2 link) | allowlist:355-367 |
| DB-011 lines this spec can retire | 6: `irradiance.cpp -> renderer.h`, `-> worldBmax`, `-> worldBmin`; `irradianceDispatch.cpp -> renderer.h`; `photonMap.cpp -> toWorld`, `-> fromWorld` | allowlist:111, 112, 125, 126, 137, 138 |
| Hiders `CRenderer::` metric | 556 references, 81 members; `irradiance.cpp` 1 line (2 refs), `photonMap.cpp` 4 refs; `photon.cpp` also reads `worldBmin/worldBmax`, `world` | `metrics-baseline.txt:59`, grep |
| Texturing `CRenderer::` metric | 0 / 0 | `metrics-baseline.txt:63` |
| `ribvector-core-residual.txt` | 9 Core files; `vector/irradianceDispatchStub.cpp` (Hiders, `irradiance.cpp`); `render/rendererStatics.cpp` needed by Geometry and by Hiders `compositor.cpp`, `irradiance.cpp`, `photonMap.cpp` | file |
| Existing tests that name the moved classes | 4 include `ri/hiders/photonMap.h` (`test_photon_map_{frame_release,sessions}.cpp`, `test_data_world_init.cpp`, `test_dataview_chunking.cpp`); none includes `irradiance.h`; 7 test doubles override `getCache` returning `CTexture3d *`; `test_irradiance_empty_string.cpp` holds `CTexture3d *cache = CRenderer::getCache(...)` | grep `tests/` |
| `sizeof(CCacheSample)` on LP64 | 232 (224 bytes of fields, `next` at offset 224) | layout of `irradiance.h:71-84` |
| `sizeof(CPhoton)` | 44 (`CMapItem` P, N, flags + C, theta, phi), pinned by `test_photon_solar0.cpp` | `photonMap.h`, `map.h` |
| Dead files under `src/ri/hiders/` | `radiance.{h,cpp}` and `harmonics.h` are compiled by no target (no CMake list names them; `CRadianceCache` derives from an undefined `CCache`); `radiance.cpp` includes `texture.h` and calls the photon map's 4-argument lookup | grep of every `CMakeLists.txt` |
| Sibling plans (030, 031, 056) | `plan.md` of each is still the unfilled template at the time of writing. Later, the coordinator's line-by-line check of 031's approved plan found that 031 (P-Q3 (a)) also deletes allowlist lines 137-138; the second spec to merge drops its duplicate edit | read 2026-11-07 |

**Maintainer answer (2026-11-07, OQ-4):** the dead `radiance.{h,cpp}` and `harmonics.h` are deleted in Phase 4
(confirmed: no CMake list compiles them).

## R1. Where the irradiance cache's storage, record and store interface live (FR-004, FR-027)

**Decision.**
- The storage moves to Texturing as `src/ri/texture/irradianceCache.{h,cpp}`: `CIrradianceCache`, still a
  `CTexture3d` (it uses the channel table, the name, `from`/`to`, `resolve`).
- Its abstract store is header-only in Texturing, `src/ri/texture/irradianceStore.h`:
  `CIrradianceStore : public CTexture3d` with three pure virtuals (`lookupCached`, `insertRecord`,
  `samplesOnMiss`), inline accessors for `from`/`to`, the record type `CIrradianceRecord`, the draw-callback type and
  the inline accessor `irradianceStoreOf(CTexture3d *)`.
- `CCacheSample` (the tree's node record) is the record's fields plus `next`, a standalone standard-layout class (see data-model.md).

**Rationale.** FR-027 allows the store's abstract class to be header-only in Texturing. Shading needs the cache both
as a store (lookup, insert) and as a `CTexture3d` (the services' `texture3dResolve` and the `dataSize` assert of the
`IDEXPR` macro). A Texturing abstract class that is a `CTexture3d` gives both through one pointer, with upcasts that
are pointer arithmetic and calls that are virtual. Shading never constructs it, so its inherited constructor and its
weak vtable are never emitted in `libshader_shading`, and Shading gains no undefined symbol (`-L link` green).

The Hiders sampler never sees the store or the record (R3), so nothing Hiders-side needs a Texturing type.

**Alternatives.**
- Record and store interface in Infra (`src/ri/core/` or `src/common/`). Rejected: Shading would hold two pointers
  for one object (the Infra interface and the `CTexture3d`), and Infra would gain GI vocabulary that only Texturing
  and Shading use.
- Shading includes the concrete `irradianceCache.h` and calls its virtuals. Rejected: it couples Shading to the
  implementation and invites a non-virtual call that would add an undefined symbol.

## R2. The shared Shading function and the hit/miss hand-off (Q1 A, FR-006, FR-027)

**Decision.** There is one function, `libshader::irradianceLookup(CShadingContext &, CTexture3d *cache, float *C,
const float *P, const float *dPdu, const float *dPdv, const float *N)`, in the new
`src/libshader/shading/irradianceCalls.{h,cpp}`. The interpreter macro `IDEXPR` (`giFunctions.h:235`) and the JIT
(`shading.cpp:3531`) both call it, in place of `cache->lookup(..., this)`, at the same point, after
`DRAW_BIND`/`bindDraws`. Its body is the old `CIrradianceCache::lookup` control flow, split at the store boundary:

1. **Point-based branch**, first and drawing nothing. If `scratch.occlusionParams.pointbased` is set and
   `scratch.occlusionParams.pointHierarchy` is non-null, zero `C[0..6]` and call the hierarchy's
   `lookupPointBased(C, P, dPdu, dPdv, N, {maxSolidAngle})` (R9). This is what `irradiance.cpp:313-319` does today.
2. **Store lookup.** `store->lookupCached(C, P, N, maxError, draw, &ctx, Pc, Nc)`:
   - The store transforms P and N into cache space (written to `Pc`, `Nc`).
   - It walks the tree exactly as `irradiance.cpp:336-422` does.
   - For every record that passes the front test, it calls `draw(&ctx)` once, at the point where
     `irradianceSampleAccept` was called.
   - On a hit it writes `C` (the envdir back in the caller's space) and returns `true`; on a miss it returns `false`
     and leaves `C` alone.
   - The callback is a plain function pointer plus a `void *`. It is Shading's own lambda over
     `CShadingContext::drawNext()`. An indirect call is no link edge.
3. **Miss.**
   - If `store->samplesOnMiss()` (`CACHE_SAMPLE`):
     - Transform `dPdu`/`dPdv` by the cache's `to`, as `irradiance.cpp:442-443` does.
     - Ask the sampler (R3) for the sample at `Pc`, `Nc`.
     - If the sampler says the record is kept, build a `CIrradianceRecord` from its result (P and N in cache space,
       `irradiance = C[0..2]`, `coverage = C[3]`, `envdir = C[4..6]`, the gradients, `dP`) and the key (R10), and call
       `store->insertRecord(record, maxError)`.
     - Then `mulmv(C + 4, from, C + 4)`.
   - Otherwise write the "no joy" value `{0, 0, 0, 1, 0, 0, 0}`.
4. `assert(dotvv(C, C) >= 0)`.

The draw sequence is therefore unchanged: one accept draw per record that passes the front test (zero-weight records
and a `smallSampleWeight` of 0 included), in traversal order, none on the point-based branch, then the sampler's draws
on the same cursor.

**Rationale.** It is Q1's option A as decided. Keeping the accept test inside the store, behind a callback called at
the same line, is the only way the draw count and order stay exact (the spec's "Draw order" edge case).

**Alternatives.**
- The store returns the candidate list and Shading applies the accept test. Rejected: it changes the traversal's
  structure and allocates per lookup.
- The callback answers accept/reject instead of a number. Rejected: the multiply by `smallSampleWeight` and the
  comparison must stay where the record's weight is computed, or the arithmetic order changes.

## R3. The sampler interface, who installs it, and the owner-less fallback (Q1 A, FR-008)

**Decision.** Shading owns `openrender::shading::IIrradianceSampler` (`src/libshader/shading/irradianceSampler.h`):

```
void sample(CShadingContext &ctx, const float *cacheFrom, const float *P, const float *dPdu, const float *dPdv,
            const float *N, IrradianceSample &out)
```

`IrradianceSample` carries `C[7]`, a `keep` flag, `gP[21]`, `gR[21]` and `dP`.

The single implementation is the Hiders class `CIrradianceSampler` (`src/ri/hiders/irradianceSampler.cpp` and `hiderIrradianceSampler.h`; the header is not named `irradianceSampler.h` because Shading owns that basename):
- Its body is `irradianceDispatch.cpp:252-587` (hemisphere, tracing through `libshader::traceRay`, environment
  lookups, photon-map reads through R6's `photonMapOf`), plus the keep decision (`maxError != 0 && coverage < 1 -
  C_EPSILON`).
- When the record is kept it also does the gradient and radius computations of `:597-608`: `posGradient`,
  `rotGradient`, `rMean * 0.5`, the `maxPixelDist` clamp.
- Neighbour clamping, the `K` scaling and the tree insertion are the store's (`insertRecord`, under the store's
  mutex, `clamp()` included).

Resolution order at a miss:
1. The context's own sampler: `CHider::CHider` installs the process-wide instance with
   `context.setIrradianceSampler()`, next to `setTracer(this)`.
2. Otherwise the process default, `CShadingContext::setDefaultIrradianceSampler()`. The renderer core installs it in
   `CRenderer::beginRenderer()` and removes it in `endRenderer()`, the same lifetime as `setDefaultServices` (gotcha
   23).
3. Otherwise: warn once per context (`CODE_NOTSTARTED`) and answer the unsampled miss.

**Rationale.**
- Q1 names the `CHider` base as the installer.
- The fallback is needed because `ShadingContract_CannedTrace{Interp,Jit}` and `ShadingContract_NoTracer` run
  `cm_occlusion`/`cm_indirect` on a context that no hider owns, inside a `RiBegin` session. Verified:
  `rendered_harness.h:38` defines `using CRProbeContext = CShadingContext;`, and `makeContext` (`:82-86`) does
  `new CRProbeContext(0)`, a bare context with no `CHider`.
  - The first asserts that diffuse-visible probe rays reached the canned tracer.
  - The second asserts exactly one `CODE_NOTSTARTED` per kind and a result of 0.
- With the process default installed at `RiBegin`, both run the same sampler code as today: the probe rays reach
  `traceRay`, which goes to the canned tracer, or warns once and misses. Both tests stay unedited.
- A services member was rejected for the same reason: the forwarder doubles (`contract_forwarder.h`) would have to
  forward it.
- The sampler is stateless (everything comes from the context it is handed), so one instance serves every thread.
- Computing the gradients outside the mutex changes timing only; at `-t:1` the order of inserts is unchanged.

**Alternatives.**
- One sampler per hider class. Rejected by Q1.
- Install only through `CHider`. Rejected: it fails the two contract tests above.
- Keep the miss path in Texturing behind a callback into Hiders. Rejected by Q1 (option B).

## R4. `getCache` keeps its return type (FR-009)

**Decision.** `CRendererServices::getCache` and `CRenderer::getCache` keep returning `CTexture3d *`. The shared
function converts with `irradianceStoreOf(CTexture3d *)`, an inline `static_cast` in `irradianceStore.h`, under a
rule written into `getCache`'s comment: getCache answers only irradiance caches. This mirrors `photonMapOf()` (spec
025). The rule is enforced in Core, which may name Texturing types: `CRenderer::getCache` converts the entry it finds in `frameFiles` with `dynamic_cast<CIrradianceStore *>` and answers `nullptr` for a name registered as another 3D kind (#104), because the unchecked `static_cast` of a wrong-kind object would be undefined behaviour in Shading. Shading cannot `dynamic_cast` (it would need `CTexture3d`'s typeinfo, an undefined symbol in `libshader_shading`), so `irradianceLookup` only handles a null cache.

**Rationale.**
- Seven test doubles override `getCache` with that return type: `contract_services.h:246`, `contract_forwarder.h:93`,
  `photon_services.h:66`, `recording_services.h:68, 164`, `table_services.h:62` and
  `recording_texture_system.h:235`. `test_irradiance_empty_string.cpp:88` stores the result in a `CTexture3d *`.
- Changing the return type would make all eight edits non-mechanical, which FR-029 forbids without approval.
- The type hazard of a name-keyed `frameFiles` entry (#104) exists today, in `rendererFiles.cpp`'s C casts. The
  `static_cast` neither adds nor removes it.

**Alternative considered (OQ-1).** Return `CIrradianceStore *` from `getCache`. This would be a breaking step within
2.x, recorded in 2.2.0, and needs the eight test edits listed above. It makes the contract explicit in the type.

**Maintainer answer (2026-11-07):** keep `CTexture3d *` and convert with `irradianceStoreOf()`.

## R5. The photon map: Texturing store, Shading access, Core bridge (FR-004, FR-005, FR-028, Q4 A)

**Decision.**

**Texturing: `src/ri/texture/photonStore.{h,cpp}`.**
- `CPhoton` keeps its members and order: 44 bytes, `test_photon_solar0.cpp` untouched.
- `CPhotonStore : public CMap<CPhoton>, public CFileResource` is today's `CPhotonMap` minus `CDataView`,
  `CMapRefTarget`, `IPhotonLookup` and every `CRenderer` read.
- It keeps every storage member: `store`, `balance`, `reset`, `write`, both lookups, `probe`/`insert` (the lookup
  octree), `bound`, `typeName`, `maxPower`, `searchRadius`, `from`, `to`, `modifying`, `mutex`.
- The constructor takes the current world matrices: `CPhotonStore(name, in, fromWorld, toWorld)`.
- `write(fromWorld, toWorld)` takes the matrices it writes.
- The direction tables (`costheta` etc., `photonMap.cpp:40-43`) move with it.
- The two unused includes (`object.h`, `raytracer.h`) go.

**Shading: `src/libshader/shading/photonMapAccess.h`** (header-only, beside `photonLookup.h`).
- `IPhotonEstimator::estimate(C, P, N, maxFound)`: today's 4-argument lookup, renamed.
- `IPhotonStore`: `storePhoton(P, N, I, C, key)`, `resetPhotons()`, `balancePhotons()`, `isModifying()`,
  `setModifying(int)`, `writePhotons(const CXform *world)`.
- `CPhotonMapRefTarget : public CMapRefTarget, public IPhotonLookup, public IPhotonEstimator, public IPhotonStore`
  (abstract).
- `photonMapOf(const MapRef &)`, a `static_cast` to `CPhotonMapRefTarget *`.

The Hiders (`photon.cpp`, the sampler; `radiance.cpp` if kept, OQ-4) include only this header.

**Core: `src/ri/render/photonMap.h`.**
- `CPhotonMap : public CPhotonStore, public openrender::shading::CPhotonMapRefTarget, public CPhotonMapView`, header
  only.
- Every override is inline and forwards to `CPhotonStore`.
- The constructor `CPhotonMap(name, in)` passes `CRenderer::fromWorld/toWorld` and calls `attach()` as today.
- `writePhotons(const CXform *world)` writes `world->from/world->to`, or `CRenderer::fromWorld/toWorld` when `world`
  is null.
- It keeps the overload `write(const CXform *)` with that same rule, for `test_data_world_init`'s `write(NULL)`.

**Rationale.**
- Q4 A fixes the Core class and its name.
- A map handle is resolved by a `static_cast` from `CMapRefTarget`, and only a class that derives from the Graphics
  State target can be that cast's destination. Shading may depend on Graphics State; Infra may not. So the hiders'
  access interfaces sit in Shading, beside `IPhotonLookup`, the spec 025 pattern of `CShaderInstance` and
  `shaderInstanceOf`.
- The interface methods have names distinct from the store's (`storePhoton`, not `store`):
  - No overload set is partially overridden, which GCC 13's `-Wall` reports as `-Woverloaded-virtual` (an error
    under `-Werror` on Linux).
  - `CPhotonMap` does not hide the store's 4-argument `store`, which `test_dataview_chunking` and
    `test_data_world_init` call.
- `photon.cpp`'s call `cMap->write(CRenderer::world)` becomes `writePhotons(CRenderer::world)`: the argument now
  carries the transform (FR-005).
  - `CRenderer::world->from/to` are the matrices `CRenderer::fromWorld/toWorld` are copied from
    (`renderer.cpp:300-303`), so the `.pm` bytes are unchanged. FR-019's byte test guards it.
  - `photon.cpp` already holds the DB-011 line for `CRenderer::world`, so no line is added.

**Why header-only.** The data viewer (`dataLoad.cpp`, compiled into `libRIBVector`) constructs the photon map. A
`render/photonMap.cpp` would add a Core file to `ribvector-core-residual.txt`, which may only shrink. Inline code
adds no Core translation unit. The `CRenderer` statics it reads are defined by `rendererStatics.cpp`, already on the
list and already read by `dataLoad.cpp`.

**Alternatives.**
- `IPhotonStore`/`IPhotonEstimator` in Infra, with Shading's target deriving from them. Rejected: two homes for one
  contract, for no gain.
- `dynamic_cast` from `CMapRefTarget *` to an Infra interface. Rejected: an RTTI cast per stored photon, and the
  spec 025 rule says a handle converts only by `static_cast`.
- `render/photonMap.cpp`. Rejected: it would grow the residual list, as above.

## R6. The data viewer's adapters, and the shape the existing tests fix (FR-014, FR-015, FR-029)

**Decision.** Four adapters in `src/ri/dataviewer/`: `photonMapView`, `irradianceCacheView`, `pointCloudView` and
`brickMapView` (`.h`/`.cpp`). Each one:
- implements `CDataView` over a non-owning pointer to the structure it draws;
- holds the viewer-only state as static members of the adapter, with the initial values the structures' statics have
  today, so state still carries from one document to the next as it does now;
- owns the key handling and the chunked draw.

How each document type reaches its adapter:
- **Photon map**: the Core `CPhotonMap` derives from `CPhotonMapView` itself.
- **Irradiance cache, point cloud, brick map**: the viewer adds a document class that derives from the structure and
  from its adapter: `CIrradianceCacheDocument`, `CPointCloudDocument`, `CBrickMapDocument`. `CDataDocument::open`
  constructs these instead of the bare structures.

**Rationale: the existing tests fix the public surface** (FR-029 allows only include and namespace edits):

| Test | Relies on | Forces |
|---|---|---|
| `test_dataview_chunking` | `new CPhotonMap(name, NULL)`; `store(P,N,I,C)` (4 args); `map->draw()` into `CDataView::install`'s sink; `CDataView::chunkSize` | `CPhotonMap` itself is a `CDataView` that draws: the Core class derives from the viewer's adapter (Core -> Consumer is allowed: `domain-rules.txt` `Core -> *`) |
| `test_data_world_init` | `new CPhotonMap(path, NULL)`; `store`; public `modifying`, `to`, `from`; `write(NULL)` reading `CRenderer::fromWorld/toWorld`; `dynamic_cast<CPhotonMap *>(doc.view())`; `CDataDocument::open` still seeding the `CRenderer` statics | the document `open()` builds for a photon map is a `CPhotonMap`; `write(const CXform *)` accepts NULL; `dataLoad.cpp`'s seeding stays (injection makes it redundant, but the test asserts it) |
| `test_data_pointcloud` | `dynamic_cast<CPointCloud *>(doc.view())`, then `typeName()`, `numChannels()`, `channelName()`, `bound()` on the `CPointCloud *` | the document object derives from both `CPointCloud` and `CDataView` (a cross cast); those four stay members of the Texturing `CPointCloud` |
| `test_data_keys`, `test_wire_cli`, `test_texture_system_null_resolver`, `plugins/texture3d/test_channels` | the public `CPointCloud` constructors and `store`; `makeBrickMap` | unchanged constructors; `CPointCloud` stays concrete |
| `test_photon_map_frame_release`, `test_photon_map_sessions` | `CRenderer::getPhotonMap` returning `CPhotonMap *`; one unambiguous `refCount`; `dynamic_cast<void *>`; `sizeof(CPhotonMap)` | one `CRefCounter` base (through `CMapRefTarget` only); polymorphic |

**What the preview observes (FR-015) must not move.** `CTexture3d::numChannels()`/`channelName()` override
`CDataView`'s virtuals today (same signatures, `CTexture3d` derives from `CDataView`). The adapters reproduce the
values exactly:

| Type | `typeName` | `numChannels` | `currentChannel` | `detailLevel` | `drawMode` |
|---|---|---|---|---|---|
| photon map | "Photon Map" | 0 | -1 | -1 | 0 |
| irradiance cache | "Irradiance Cache" | 3 | -1 | -1 | 0 (its `drawDiscs` drives the draw, not `drawMode`) |
| point cloud | "Point Cloud" | the channel count | `drawChannel` | -1 | `drawDiscs` |
| brick map | "Brick Map" | the channel count | `drawChannel` | `detail` | `drawType` |

SC-004's recorded `DataScene` (T-B) pins all of this.

**Alternatives.**
- Pure composition (`doc.view()` returns an adapter that owns the structure). Rejected: `dynamic_cast` in two
  existing tests would answer null.
- Editing those tests to name the document classes. Rejected: not mechanical (FR-029).

**Maintainer answer (2026-11-07, OQ-2):** yes, the core `CPhotonMap` derives from `CPhotonMapView`.

## R7. The read interfaces for the channels and the viewer (FR-011)

**Decision.** Narrow public operations, shared by the remote channels and the adapters:

- **`CIrradianceCache`**:
  - `template <class F> void forEachNode(F &&visit) const`, the exact stack traversal of
    `remoteChannel.cpp:521-549`: root first, children pushed 0 to 7, popped last-in-first-out. `visit` receives the
    node's record list head.
  - `forEachRecord`, the same traversal flattened.
  - `rootCenter()`, `rootSide()`, `setRoot(center, side)`.
  - `insertReceived(const CCacheSample &)`: copy into the cache's memory and place by the record's `dP` with no
    clamp, no `K` and no lock, exactly as `remoteChannel.cpp:578-622`.
  - `treeDepth()` (named so because the member `maxDepth` exists; approved after Phase 3).
- **`CPointCloud`**: `points()` (`items + 1`), `pointData()` (`data.array`) and `numPoints()` (existing
  `getNumPoints`), with the existing `store` and `dataSize`.
- **`CTexture3d`**:
  - `defineChannels(const char *)` moves from protected to public.
  - New `setTransform(from, to)`, `setDPScale(float)`, and `channelSampleStart(i)`/`channelNumSamples(i)` (the
    brick view reads `channels[drawChannel].sampleStart/numSamples`).
- **`CBrickMap`**: `template <class F> void visitVoxels(int level, F &&visit)`, which calls `findBrick(x, y, z,
  level, FALSE, NULL)` in the draw's `xe, ye, ze, zi, yi, xi` order and loads bricks on demand as the old draw does.
  `visit` receives the voxel centre, the voxel size, its data, its normal and its weight. Plus `maxLevel()`,
  `sideLength()` and the existing `bound`.

**Rationale.**
- The templates live in Texturing headers. Core and Consumer code instantiates them, so no Texturing internal type
  appears in the caller's code and no new link symbol arises.
- The traversal orders are copied, not reinvented, so the wire bytes and the primitive batches stay the same.
- The brick map's paging (`findBrick`, `loadBrick`, the LRU counters) stays private; the visitor is the only way in
  (the spec's lazy-draw edge case).

**Alternatives.**
- Make the node and brick structs public and let callers walk them. Rejected: it exposes the paging and the tree
  layout.
- Keep the friends. Rejected by FR-011.

## R8. Remote channels (FR-011, FR-012)

**Decision.** `CRemoteICacheChannel` and `CRemotePtCloudChannel` (Core) use only R7's public operations.
- **Send**: unchanged in content and order. The cache sends `sizeof(CCacheSample)` raw bytes per record, so after R10
  the record carries its key on the wire.
- **Receive**:
  - The cache channel receives each batch into a temporary buffer, clears `next`, then calls `insertReceived` per
    record in arrival order. The linked-list order, and so the summation order of later lookups, is unchanged.
  - The point-cloud channel's `setup` calls `defineChannels`, `setTransform(identity, identity)` and `setDPScale(1)`.

`CRemoteICacheChannel`'s constructor still takes the concrete cache (`texture/irradianceCache.h`, Core -> Texturing,
allowed).

**Rationale.** FR-011. The batch allocation in the old receive was an internal detail: one `memory->alloc` for a
batch versus one per record changes no result.

**Alternatives.** Moving the channels into Texturing. Rejected: they use sockets and the renderer's channel registry,
which belong to the core.

**Maintainer answers (2026-11-07):**
- OQ-5: T-A pins the receiver's written tree against the base; T-A2 compares lookups on the finished tree and
  reports differences without fixing them.
- OQ-8: T-A masks the `CPointCloudPoint` padding as a third mask. `.pm` files are compared field by field and cache
  files by SHA-256. Every base recording is taken twice and must agree.

## R9. `CTexture3d` and the 3D family's ties (FR-001, FR-007, FR-011, FR-013, Q6)

**Decision.**
- `texture3d.h` drops the `CDataView` base and `dataView.h`, the `CShadingContext` forward declaration and the
  `CRemotePtCloudChannel` friend.
- The six-argument virtual becomes `virtual void lookupPointBased(float *C, const float *P, const float *dPdu,
  const float *dPdv, const float *N, const CPointBasedLookup &opt) = 0;`, where
  `struct CTexture3d::CPointBasedLookup { float maxSolidAngle; }`.
  - `CPointHierarchy` implements it. Its body is today's, with `options.maxSolidAngle(context)` replaced by
    `opt.maxSolidAngle`.
  - `CPointCloud`, `CBrickMap` and `CIrradianceCache` keep the `assert(FALSE)` override under the new name.
- `CPointHierarchy::Options` and its constructor argument are deleted, with `rendererFiles.cpp`'s
  `pointHierarchyMaxSolidAngle`.
- Every structure keeps its `typeName()` and `bound()` as plain Texturing members. `test_data_pointcloud` calls them,
  and they describe storage, not viewing.
- `CPointHierarchy`'s empty `draw`/`bound`/`typeName` go.
- `pointCloud.h` and `brickmap.h` lose `draw`, `keyDown`, `currentChannel`, `detailLevel`, `drawMode` and the four
  viewer statics, which move to R6's adapters.
- `CBrickMapGeometry`'s forward declaration and friend go (Q6). `makeBrickMap`'s friendship stays (FR-013).
- `CPointCloud::lookup`/`store` and the hierarchy's traversal bodies are not edited (FR-007, FR-030).

**Rationale.**
- A distinct name avoids an overload set split across the 4-argument `lookup` and the reshaped one (the
  `-Woverloaded-virtual` risk of R5).
- The per-lookup struct carries the solid angle per call, not per hierarchy (the spec's point-based edge case), and
  leaves room for spec 053 without another reshape.

**Alternatives.** A bare `float maxSolidAngle` parameter. Workable; the struct was kept for extension without a
signature change.

## R10. Keys for spec 040 (FR-017, FR-018, FR-019, Q3 B)

**Decision.** Both keys are 64-bit, made with the SplitMix64 `mix` of `common/sampling.h`; neither draws a random
number.

- **Record key**: `irradianceRecordKey(ctx.boundDraws())` = `mix(0x1CAC4E5A, seedHash, indexBase)` of the sub-stream
  `bindDraws()` bound for the call.
  - That sub-stream is a pure function of the shading point's key, the call site, the occurrence and the ray depth
    (`shading.h:510-530`).
  - It is computed in the shared Shading function (R2) and stored in `CIrradianceRecord::key`, the field after `dP`:
    offset 224, so `next` moves to 232 and `sizeof(CCacheSample)` is 240 on LP64.
  - The cursor is copied at the function's entry, before the store lookup draws and before the sampler traces.
    `keyedNext()` advances only `drawCursorDim`, not `drawCursor` (`shading.h:1041`), but a nested `bindDraws` or a
    reset (`shading.cpp:806, 846`) during the sampler's work could rebind the cursor. The copy keeps the key
    independent of cache occupancy, and so of thread timing. Phase 7 confirms which paths touch the cursor.
  - It is sent on the wire with the record. It is not written to cache files: `writeNode`/`readNode` stay field by
    field, and read records get key 0.
  - `CShadingContext` gains a public `boundDraws()` accessor (Shading internal).
- **Photon key**: `photonKey(pointKey.full, bounce)` = `mix(0xF07011BE, pointKey.full, bounce)`.
  - `pointKey` is `currentShadingState->pointKeys[v]`, the identity `photon.cpp:207-212` derives (light, index in the
    frame's sequence, frame, seed).
  - `bounce` is `numDiffuseBounces + numSpecularBounces` at the store.
  - `tracePhoton` gains the point key as an argument; its four call sites pass `pointKeys[numVertices - i]`, the index
    `bindDraws` already uses.
- **Where the photon key lives.** In `CPhotonStore::keys`, a side array indexed like `items`:
  - `store` grows it in step with `CMap::store`.
  - `reset` clears it.
  - `read` fills it with 0 (no file carries a key).
  - The dummy photon of an empty `balance()` gets 0.
  - `CMap::balance` permutes it through a new protected virtual hook in `core/map.h`,
    `permuted(T *const *sorted, const T *oldItems, int count)`, with an empty default. The hook is called after the
    reordered array is built and before the old one is freed.
  - `CPointCloud`, `CPointHierarchy` and `CDepository` do not override the hook, so the cost elsewhere is one virtual
    call per balance.
- **Exposure.** `CIrradianceCache::forEachRecord` gives `record.key`; `CPhotonStore::photonKey(i)` gives the photon
  keys.

**Rationale.**
- Q3 B: the record key in memory and on the wire, the photon key in a side array, no file change.
- A record key built from the bound sub-stream is unique per (point, call site, occurrence, depth). This is exactly
  the identity that already makes the sampler's draws thread-independent (spec 039).
- A photon is stored once per map per bounce, so `(pointKey, bounce)` is unique within a map.

**Alternatives.**
- 32-bit keys. Rejected: collisions in a frame of 10^6 records are likely.
- The key inside `CPhoton`. Rejected by Q3 (44-byte layout).
- A bucket key. Rejected: buckets do not exist under the raytracer, and a bucket holds many records.

**Spec point that cannot hold as written (OQ-3).** US5's Independent Test asks that the set of `(key, record)` pairs
be the same at different thread counts. For photons that holds: each photon's path comes from its own identity. For
irradiance records it cannot: which shading points miss the cache depends on what other threads inserted first, so
the record set itself varies with `-t`. That variation is spec 040's subject. The key's own property, "a record
created at a given sample has the same key at any thread count", holds.

The plan's key test (T-E) therefore checks:
- photons: the same set at `-t:1`, repeated, and at `-t:4`;
- records: the same set at `-t:1`, repeated, and the per-sample key property.

**Maintainer answer (2026-11-07, OQ-3):** US5's Independent Test is corrected as proposed.
- Photons: the same set across runs and thread counts. The cross-thread comparison is opt-in (`gi_split_threads`).
- Records: the same set across repeated `-t:1` runs, and the same sample gives the same key at any thread count.

## R11. Lock discipline and the TSan build (edge cases, Q8)

**Decision.**
- `insertRecord` takes the store's mutex around exactly what `irradianceDispatch.cpp:592-666` held it for: the
  allocation, `clamp()`, the depth computation and the link.
- Lookups stay lock-free, as today ("if word-writes are atomic").
- `CPhotonStore::store` and the probe `insert` keep their locks.
- Nothing is added for TSan. Q8 left #122's investigation out; the TSan tree must keep building, and Phase 8 builds
  it once.

**Rationale.** The spec's edge cases. The gradients move outside the lock, which is timing only.

## R12. Files and the services version (FR-009, FR-019)

**Decision.**
- `kRendererServicesVersion` 2.1.0 becomes 2.2.0, a MINOR step that breaks nothing. The history entry lists:
  - Added: `photonMapAccess.h` (`IPhotonEstimator`, `IPhotonStore`, `CPhotonMapRefTarget`, `photonMapOf`; the
    4-argument photon lookup is now `IPhotonEstimator::estimate`).
  - Added: `irradianceSampler.h` (`IIrradianceSampler`, `IrradianceSample`); `CShadingContext::setIrradianceSampler`,
    `setDefaultIrradianceSampler` and `boundDraws`; `irradianceCalls.h` (`libshader::irradianceLookup`).
  - Wording: `getCache` answers only irradiance caches, converted with `irradianceStoreOf()`.
- The implementers' `static_assert(MAJOR == 2)` are unchanged.
- No file format changes. At `-t:1`, cache files are byte-identical and `.pm` files are identical in every defined
  byte (T-F, OQ-8).

**Rationale.** FR-009. With R4's decision no member signature changes.

**Collision note.** 031 may also step the version (its forwarding entry, `IShadingProducer`). Specs of one round may
not depend on each other, so each steps from its own base. The second to merge renumbers to the next MINOR and merges
the history entries at rebase. This is a textual resolution with no content change, recorded in both plans.
**Maintainer answer (2026-11-07, OQ-7):** agreed. DB-017 follows the same second-to-merge rule as DB-025 (OQ-6).

## R13. `libRIBVector` and the residual list (FR-008, SC-008)

**Decision.**
- `src/ri/CMakeLists.txt`:
  - `ri_sources` loses `hiders/irradiance.cpp`, `hiders/irradianceDispatch.cpp` and `hiders/photonMap.cpp`.
  - It gains `texture/irradianceCache.cpp`, `texture/photonStore.cpp`, `hiders/irradianceSampler.cpp`, the four
    `dataviewer/*View.cpp` and `libshader`'s `irradianceCalls.cpp` (in `libshader_shading`'s list).
  - `ribVector_sources` swaps `hiders/irradianceDispatch.cpp` for `hiders/irradianceSampler.cpp` in its REMOVE list
    and drops `vector/irradianceDispatchStub.cpp` from its APPEND list.
- `ribvector-core-residual.txt`:
  - The stub's row is deleted.
  - In `rendererStatics.cpp`'s "needed by" cell, `irradiance.cpp, photonMap.cpp` are replaced by
    `dataviewer/dataLoad.cpp` and `render/photonMap.h` (inline), the true remaining readers.
  - Nothing is added.

**Rationale.** FR-008, SC-008, and "may only shrink". The cell names no parse/output/preview file, so the check's rule
holds.

**Overlap.** 030 (hcshader) and 031 (Geometry names, three stubs) edit the same file and row. The second to merge
resolves the text.

## R14. Performance (SC-007, Q9 B)

**Decision.** This is spec 029's method (its research R14). There are three scaled scratch scenes under
`specs/032-gi-cache-split/tools/`:
- `perf-occlusion-cache.rib`: `sphere-occlusion-reyes` at 640x480, `ShadingRate 1`, a written cache with
  `maxerror > 0`.
- `perf-indirect-photon.rib`: a photon pass writing a global map, then `indirectdiffuse` reading it, two frames. The
  photon frame is pinned to one thread (`Option "limits" "numthreads" [1]`) because of the intermittent
  `CPhotonHider` crash; only the `indirectdiffuse` frame runs at the series' thread count.
- `perf-ptc.rib`: `bake3d` in frame 1 and `texture3d` in frame 2, so the #122 race cannot occur within a frame.

The method:
- Five interleaved series per platform, at `-t:4`, against a frozen `build-base` (the base binaries with install
  names rewritten, spec 029 DEP-9).
- The bar is the run-to-run spread measured on the starting tree.
- `tools/timing_sc007.sh` produces the scenes, runs the series through `cpujob` and prints the medians and spreads.

**Rationale.**
- The added costs: one indirect call per record that passes the front test (it replaces a direct call), two virtual
  calls per cache lookup, and four virtual calls per stored photon (`isModifying`, `storePhoton`, and on first use
  `setModifying`/`resetPhotons`).
- Q9 corrected the 2% attribution: spec 039 replaced 2% with per-scene budgets, and no existing performance scene
  exercises GI.

## R15. Issues near this work (not absorbed)

- **#122**: `pointCloud.h`/`.cpp` and `texture3d.h` change layout and vtable (the lookup signature, the viewer base),
  which may shift the race's timing. The bodies stay unchanged (FR-030), a `bake3d` crash in validation is re-run in
  isolation, there is no crash-rate criterion, and no fix is claimed.
- **#131**: the point-cloud channel's loopback (T-A) sends an in-memory cloud. If `numItems` already miscounts there,
  T-A records it as the base does; nothing is fixed.
- **#104**: the GI entries of `frameFiles` are still keyed by name only (`getPhotonMap`, `getCache`, `getTexture3d`),
  so one name used for two GI kinds still answers the wrong class. Unchanged, and recorded in the photon-map
  contract.
- **#145, #146, #97**: the JIT's `photonmap()` estimator and optional lists are untouched. `photonmap()` keeps calling
  `IPhotonLookup::lookup`; parity is unchanged.
- **#15**: still looks stale (`numChannels()` is already called). Not this spec's.
