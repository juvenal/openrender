# Contract: tests, recordings and the TEST-APPROVAL inventory

**Requirements**: FR-012, FR-019, FR-024, FR-026, FR-029; SC-003 to SC-007, SC-009. **Research**: R3, R6, R10, R14.

Nothing listed here is applied before the maintainer approves it at the test-approval pause, with three exceptions,
which the maintainer approves together with this plan:
- the base-tree recordings (T-A, T-B, T-F), which must exist before any `src/` edit;
- the image and sweep manifests;
- the timing baseline.

Recordings are taken on the unmodified tree at `e83d005d` on macOS and on Linux.
- Each is taken twice and must agree before it is staged (indeterminate bytes, OQ-8).
- Re-recording stays allowed until Phase 3 begins, in case the approval pause changes a mask.

## Existing tests that must pass unchanged (except the edits in "Mechanical edits")

| Test(s) | What they pin |
|---|---|
| `test_dataview_chunking`, `test_data_world_init`, `test_data_pointcloud`, `test_data_keys`, `test_data_disk_expand`, `test_debugdump_roundtrip`, `test_wire_cli`, `test_preview_*`, `Preview_*` | the viewer's public surface (research R6's table) |
| `photonmap_FrameCacheRelease`, `photonmap_TwoFrameSession`, `photonmap_ObjectDefinitionRelease` | #64 lifetime: `getPhotonMap` returns `CPhotonMap *`, one `refCount` |
| `PhotonSolar0_*` (`test_photon_solar0.cpp`, not edited) | the 44-byte `CPhoton` and the `.pm` layout |
| `ShadingContract_CannedTrace{Interp,Jit}`, `ShadingContract_NoTracer` | the owner-less context's irradiance sampling (research R3) |
| the spec 026 trace replay (`tests/shading_contract/replay/`) | every probe ray of `occlusion()`/`indirectdiffuse()`, recorded |
| `test_irradiance_empty_string`, `test_texture_system_null_resolver`, `plugins/texture3d/test_channels` and the `plugin_abi` texture3d characterization | `getCache`, the point-cloud constructors, the channels |
| `-L architecture`, `-L link` (`Link_*`, `check_scope` S1b, `check_shading_self_contained`), `SceneInterface_RibVectorResidual` | the boundary and `libshader_shading`'s imports |

## GI test set (SC-003)

Each test is byte-identical to the same platform's master baseline at the same pins, with every `MaxBlockAvgDiff`
unchanged.

- `Visual_sphere-{occlusion,indirectdiffuse,photonmap,texture3d-bake3d,gather}-reyes` and their `-slo` variants.
- `Visual_photon-parity`, `Visual_photon-parity-slo`, `Visual_photon-solar0-*`.
- `Visual_subdiv-{facevarying-seam,hierarchical-override,loop,new-tags}-photon`.
- `SamplingJitDeterminism_{occlusion,indirectdiffuse,gather}`, `OcclusionSampler_NetCount_*`, `OcclusionGolden_*`.
- The two `bake3d` scenes of the determinism sweep, at the sweep's pins.
- `photonmap_*`, `PhotonSolar0_*`, the `plugin_abi` texture3d characterization.

The exact list is the manifest Phase 1 writes: every test whose scene or shader names `occlusion`,
`indirectdiffuse`, `photonmap`, `bake3d`, `texture3d`, `globalmap`/`causticmap` or `Hider "photon"`. A `bake3d` crash
(#122) is re-run in isolation and is not judged a regression.

## New tests (TEST-APPROVAL)

| ID | Name / label | What | Recorded on base? | Red until |
|---|---|---|---|---|
| T-A | `GiRemote_LoopbackCache`, `GiRemote_LoopbackPointCloud` (label `gi_split`) | Drives `sendSetupData`/`setup` and `sendRemoteFrame`/`recvRemoteFrame` of both channels over `socketpair()` in one process, with the arena initialised and the `CRenderer` statics seeded as `dataLoad.cpp` seeds them. The sender cache is constructed from fixture F1 (`CACHE_READ | CACHE_RDONLY`). The receiver is a fresh cache (`CACHE_WRITE`, scratch name) whose root `setup()` overwrites. The clouds are built by the public constructor and `store`. Checks: the masked sent bytes against `tests/gi_split/reference/<platform>/loopback-*.bin`, the receiver's written cache file (on destruction) against the reference, and the receiving cloud's points, data and channels against the sender's. After Phase 7 the comparison strips the key field (`remote-channels.md`) | yes, before any `src/` edit | green on base; must stay green. Its source has one adaptation after Phase 4 (M5) |
| T-A2 | `GiRemote_LoopbackLookup` and `GiCache_KindMismatch` (`gi_split`) | Finished tree only: receiver `lookupCached` against sender at the sender's record positions, counting draws. `GiCache_KindMismatch` is a second CTest name of the same source: a name registered as a point cloud and then asked of `getCache` answers `nullptr` (Core's kind check, M3), and a cache obtained under another name is returned unchanged by a second call | no (no context-free lookup on base) | Phase 4 (enabled when `lookupCached`, `insertReceived` and the `getCache` kind check exist; re-run in Phase 6) |
| T-B | `Preview_DataSceneUnchanged` (`preview`) | Through the `ribdata_*` C API (unchanged on both trees): the `DataScene` of the 5 document types (type name, channel count and names, current channel, detail level, draw mode, bound, every primitive batch as count + hash, chunking) before and after each key of `test_data_keys`'s sequences. Fixtures are generated at run time: the photon map written by an `orender` photon pass (header-free: T-B includes no photon-map header, so it is not among M1-M4), the point cloud and brick map as in `test_data_keys`, the cache from F1, the debug dump as in `test_debugdump_roundtrip`. Compared with `tests/preview/reference/<platform>/datascene.txt` | yes | green on base; must stay green |
| T-C | `TextureOnly_GiStores` (`texture_only`) | SC-006: a program on the Texturing-only include path builds a photon store and an irradiance cache, fills, writes, reloads and queries both, with equal results before and after the reload | no (the classes do not exist) | Phases 4 and 5 |
| T-D | `TextureOnly_IrradianceDrawOrder` (`texture_only`) | FR-006 at the store: a cache with records in front of, behind and beside the query, some of zero weight, `CACHE_SAMPLE` on and off. The draw callback records its call sequence, which must equal exactly the records that pass the front test, in traversal order (expected list computed by the test from the same geometry) | no | Phase 4 |
| T-E | `GiKeys_Photons`, `GiKeys_IrradianceRecords` (`gi_split`); `GiKeys_PhotonsThreads` (opt-in label `gi_split_threads`) | `gi-keys.md` properties 1 and 2, one in-process render per process (#2). The everyday tests compare repeated `-t:1` runs. The `-t:1` against `-t:4` photon comparison is opt-in: the intermittent `CPhotonHider` crash and the unguarded `modifying` reset make it unfit for the everyday run (OQ-3). Photons are compared field by field (`P`, `N`, `flags`, `C`, `theta`, `phi`), never as raw bytes (OQ-8) | no | Phase 7 |
| T-F | `GiFiles_Unchanged` (`gi_split`) | FR-019. The `.pm` files written by the `photon-parity` photon pass at `-t:1` are compared **field by field**: the header, `numItems`, `maxItems`, items 1 to n by field, `bmin`/`bmax`, both matrices and `maxPower`. Item 0, the `CPhoton` padding and an empty map's dummy direction are skipped (OQ-8). The cache written by `sphere-occlusion-reyes` with `"w"` mode at `-t:1` is fully defined, so its SHA-256 is compared. The references are `tests/gi_split/reference/<platform>/files.txt` | yes, twice, the two agreeing | green on base; must stay green |
| T-G | `GiSplit_Boundary` (`architecture`; a `cmake -P` script like spec 038's checks) | SC-001 and SC-008 by text: no forbidden name or include under `src/ri/texture/`; no `friend` naming `CRemote*`/`CBrickMapGeometry` under `src/ri/texture`/`src/ri/hiders`; no `src/ri/texture` include in any file under `src/ri/hiders` (the dead `radiance.{h,cpp}` and `harmonics.h` are deleted, OQ-4); `irradianceDispatch.cpp`, `irradianceDispatchStub.cpp`, `irradianceSampleAccept`, `radiance.cpp`, `radiance.h` and `harmonics.h` absent | no | Phases 3 to 5 |

The red-first targets (T-A2, T-C, T-D, T-E) are all defined in `tests/gi_split/CMakeLists.txt` behind `OPENRENDER_032_RED_TESTS`; M6a later re-defines T-C and T-D in `tests/unit/texture_only/CMakeLists.txt`. Red evidence is taken in a scratch build tree (`build-red032`) by building only the test objects, never in the main `build`. T-G reads its enabled sub-checks from the cache variable `GI032_BOUNDARY_CHECKS` (default empty).

Fixture **F1** (`tests/gi_split/scenes/icache-fixture.rib`, with the shader `occlusion_probe`) is a CTest
`FIXTURES_SETUP` that renders with `orender` at `-t:1` and writes a small irradiance cache. T-A, T-A2 and T-B use it.
On the base tree the cache can only be filled by a render: insertion there is private or `sample()`-only.

## Mechanical edits to existing tests (TEST-APPROVAL; FR-029)

| ID | File:line | Edit |
|---|---|---|
| M1 | `tests/unit/photon_map/test_photon_map_frame_release.cpp:44` | `#include "ri/hiders/photonMap.h"` becomes `#include "ri/render/photonMap.h"` |
| M2 | `tests/unit/photon_map/test_photon_map_sessions.cpp:45` | same |
| M3 | `tests/preview/test_data_world_init.cpp:7` | same |
| M4 | `tests/preview/test_dataview_chunking.cpp:5` | same |
| M5 | the new T-A source, after Phase 4 | `#include "ri/hiders/irradiance.h"` becomes `#include "ri/texture/irradianceCache.h"`, and the two cache constructions gain the explicit world-bound arguments (`CRenderer::worldBmin/worldBmax`, the values the base constructor read). This is a new test, so FR-029 does not bind it; the adaptation is listed so the maintainer approves it with the test. The references are not re-recorded |
| M6 (applied as M6a in Phase 4 and M6b in Phase 5) | `tests/unit/texture_only/CMakeLists.txt` | Adds a second library on the same narrow include path: `irradianceCache.cpp`, `photonStore.cpp`, `texture3d.cpp`, `texture3dChannels.cpp`, plus a staging copy of `src/ri/core/map.h` beside `ri_config.h`. `src/ri/core` stays off the path, as the file's comment requires. Also the two executables T-C and T-D. The existing `TextureOnly_Standalone` target is untouched. M6a (Phase 4): `irradianceCache.cpp`, `texture3d.cpp`, `texture3dChannels.cpp`, the `map.h` staging copy and the T-C/T-D executables; M6b (Phase 5): `photonStore.cpp` |

No other existing test changes. `test_photon_solar0.cpp`'s comment naming `src/ri/hiders/photonMap.h` becomes stale
and stays (FR-019 forbids editing it). OQ-1 was answered "keep `CTexture3d *`", so no test double is edited.

**Approval status (2026-11-07).** The maintainer approved T-A to T-G, F1 and M1 to M6 at plan level, with OQ-5 and
OQ-8 as recommended: T-A has three masks, and every base recording is taken twice and must agree. Each test is still
shown at the implement stage's test pause before it is applied.

## Platform runs (FR-024, SC-009)

- macOS (Apple Clang, vcpkg) and Linux (Ubuntu 24.04, GCC 13, `-Werror`, over ssh with the remote `cpujob -j2` in a
  per-spec tree).
- The set: `-L architecture`, `-L visual`, `-L shading_parity`, `-L texture_tile`, `-L texture_only`, `-L state`,
  `-L plugin_abi`, `-L link`, `-L preview`, `-L gi_split`. Then the everyday `-LE determinism-sweep -j4`, and last
  the determinism sweep at `-j2` against the Phase 1 manifests.
- Builds: the no-JIT build and the no-OpenEXR build (if configured) once each; the TSan tree builds once (no test run
  for #122, Q8).
