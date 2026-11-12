# Registry draft: spec 032 edits to `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`

Shown for approval at PAUSE 6a (T099/T100). **Not applied.** `<date>` is the date the maintainer approves it. Nothing here depends on another spec's merge.
No edit to `CLAUDE.md` or `DEVNOTES.md` is proposed in this phase. Measured facts: allowlist 335 -> 319 in Phases 3 to 5 (DB-010 8 -> 0, DB-011 164 -> 158, DB-025 12 -> 1; DB-004 52, DB-006 30, DB-017 78 unchanged);
`archcheck` Hiders `CRenderer::` metric 556/81 -> 549/78, Texturing 0/0, TOTAL 604/89 -> 597/86.

## 1. DB-010: move to **Resolved** (the whole entry moves under `## Resolved`, with these changes)

- **Tracking** line: `remediation spec 032 (#38); spec 027 (#33) removed the rest`.
- **Size**: `8 edges (5 include, 3 link)` becomes `0 edges (spec 032; 8 before)`.
- **History** gains: `spec 032 (<date>): 8 -> 0 edges, the 8 allowlist lines and the section header deleted (nothing added).`
- The spec 027 History line's sentence "Texturing now includes nothing from another domain except `texture3d.cpp`'s `renderer.h` and `displayChannel.h`" is **corrected**: spec 037 had already removed both
  (`texture3d.cpp` includes neither since 2026-10-31, DB-021), and the Texturing metric row is 0 / 0.
- Appended:
  `- **Resolved** by spec 032 (specs/032-gi-cache-split/), <date>: the irradiance cache is passive storage in Texturing (src/ri/texture/irradianceCache.{h,cpp}, irradianceStore.h) and the sampler stays in Hiders
  (src/ri/hiders/irradianceSampler.cpp), reached through Shading's libshader::irradianceLookup and IIrradianceSampler. No Hiders file includes a Texturing header or links a Texturing symbol (GiSplit_Boundary: hiders-no-texture).`

## 2. DB-020: move to **Resolved**

- **Evidence** is rewritten to the end state: the `CShadingContext` friends were removed by spec 029 (2026-11-04); `CRemoteICacheChannel`, `CRemotePtCloudChannel` and `CBrickMapGeometry` are no friend of any class (spec 032, Phase 3 and 4);
  the only `friend` under `src/ri/texture` and `src/ri/hiders` is `friend void makeBrickMap(...)` (`pointCloud.h`, FR-013: a free function of Texturing itself, not a cross-domain declaration).
- **Tracking** line corrected: `remediation specs 030 (#36), 032 (#38)` becomes `spec 032 (#38)`: spec 030 has no friend left to remove, because spec 029 removed the light friends.
- Appended: `- **Resolved** by spec 032 (specs/032-gi-cache-split/), <date>: the three remote-channel friends and both CBrickMapGeometry lines are gone (the channels and the viewer read the stores through public accessors and adapters). GiSplit_Boundary (friends-texture, friends-hiders) guards it.`

## 3. DB-025: history entry, stays **Open** for `surface.cpp`

`surface.cpp -> debug.h` is still in the allowlist (`DB-025 include src/ri/geometry/surface.cpp -> src/ri/dataviewer/debug.h`), so the entry stays Open (spec 031's line).
- **Size** `12 edges (4 include, 8 link)` becomes `1 edge (include)`.
- **Evidence**: Texturing and Hiders no longer appear: only Geometry's `surface.cpp` includes `debug.h`.
- **History**: `spec 032 (<date>), 12 -> 1: 11 allowlist lines deleted. Texturing's pointCloud/brickmap/texture3d and the cache lost their CDataView base, draw and keyDown (nine lines, Phase 3);
  the photon map's dataView.h include and CDataView::drawPoints link (two lines, Phase 5). The viewer owns the adapters (src/ri/dataviewer/{pointCloudView,brickMapView,irradianceCacheView,photonMapView}).`

## 4. DB-017: history entry, stays **Open** (78 lines)

The other three pairs (`geometryDispatch`, `rendererDeclarationsDispatch`, `surfaceCrossingDispatch`) are still in this tree, so this spec does not resolve it.
- **Evidence**: "four pairs" becomes "three pairs" and the `hiders/irradianceDispatch.cpp` / `vector/irradianceDispatchStub.cpp` pair is dropped from the list.
- **History**: `spec 032 (<date>): the irradiance pair is deleted (hiders/irradianceDispatch.cpp became hiders/irradianceSampler.cpp, which ribVector leaves out and needs no stand-in for; vector/irradianceDispatchStub.cpp is deleted
  and its row left tests/architecture/ribvector-core-residual.txt). It had no allowlist line, so the count stays 78. The spec 038 History line naming hiders/irradiance.cpp for the stub is superseded.`

## 5. DB-011: history entry

- **Evidence/Size**: the Hiders metric reads `549 references (was 556)` and the DB-011 count `158 lines` (was 164; 160 after Phase 4's four, 158 after Phase 5's two).
- **History**: `spec 032 (<date>): the world bound and the world matrices are injected into the stores (CIrradianceCache takes worldBmin/worldBmax, CPhotonStore takes the creator's fromWorld/toWorld) instead of read from CRenderer, and
  irradianceDispatch.cpp, which included renderer.h, became the sampler without that include. 6 allowlist lines deleted: irradiance.cpp -> renderer.h, irradianceDispatch.cpp -> renderer.h, irradiance.cpp -> CRenderer::worldBmax and ::worldBmin,
  photonMap.cpp -> CRenderer::toWorld and ::fromWorld. 164 -> 158 lines. The Hiders CRenderer:: metric fell from 556 to 549 references and 81 to 78 members.`
- The spec 038 History line's "needed by hiders/compositor.cpp, hiders/irradiance.cpp and hiders/photonMap.cpp" is updated: `rendererStatics.cpp` is now needed by `hiders/compositor.cpp`, `dataviewer/dataLoad.cpp` and the inline
  `render/photonMap.h` (the residual-list cell).

## 6. DB-024: history entry

Measured by `archcheck` (`CONSUMER` lines of `src/ri/dataviewer`, base against Checkpoint 5): at the base `dataLoad.cpp` reached Hiders (`irradiance.h`, `photonMap.h`) and Texturing (`brickmap.h`, `pointCloud.h`) and Scene Interface (2);
now `dataLoad.cpp` reaches only Scene Interface (2), and the Texturing reach sits in the viewer's own adapters (`brickMapView.h -> brickmap.h`, `pointCloudView.h -> pointCloud.h`, `irradianceCacheView.h -> irradianceCache.h`,
`photonMapView.cpp -> photonStore.h`). The total stays 71 consumer-reach edges and the check never fails on them.
- **Evidence**: the line "`src/ri/dataviewer/dataLoad.cpp` reaches into Hiders (2) and Texturing (2) and includes 2 Scene Interface headers ..., and `renderer.h` for the `CRenderer::fromWorld` seeding (DB-025)" becomes: `dataLoad.cpp` includes 2 Scene
  Interface headers, `renderer.h` (the `CRenderer` seeding) and the Core `photonMap.h`; the viewer's adapters (`pointCloudView`, `brickMapView`, `irradianceCacheView`, `photonMapView`) reach Texturing (4).
- **Direction**: "the data viewer's Hiders and Texturing reach (spec 032)" is struck as done.
- **History**: `spec 032 (<date>): the data viewer's Hiders reach is gone. It owns the adapters through which it draws the 3D family, the irradiance cache and the photon map (they used to derive from CDataView), so the viewer depends on Texturing and not the other way round.`

## 7. DB-023: no change.

## Checks that read the registry

`archcheck` reads the registry only for the IDs of Resolved entries named by the allowlist: no allowlist line names DB-010 or DB-020, so moving them is consistent (the check is re-run after the edit, T100).
