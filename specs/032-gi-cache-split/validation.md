# Validation record: spec 032, GI Cache Split

Worktree `/Volumes/Projects/Development/CLI/openrender-worktrees/032-gi-cache-split`, branch `032-gi-cache-split`.
Base for every `git diff <base>`: master `e83d005d` (HEAD `445ee81f` = `e83d005d` + the design commit `c4e1818c` + the tasks commit).

## §0 Orientation and starting-tree measurements

### T002 orientation (2026-11-09)

- `git status --short` at the start: clean. `git log -3 --oneline`: `445ee81f docs: add spec 032 tasks and analyze fixes`,
  `c4e1818c docs: add spec 032 GI cache split design`, `e83d005d Merge branch 'docs/architecture-revamp-brief'`.
  `.specify/feature.json` names `specs/032-gi-cache-split`.
- `git check-ignore build build-base build-logs build-nojit build-tsan`: all five ignored (`.gitignore` has `build-*`).
- CLAUDE.md "Spec-kit agent rules": both copies (this worktree and the main checkout, the first entry of `git worktree list`)
  carry the section; `diff` of the two files is empty, same last-commit time (1793933858), no uncommitted change. The worktree
  copy was followed.
- Disk: `df -h /Volumes/Projects` 6.3 GiB free at the start (three other specs build in parallel); 5.0 GiB after the base build.
- Host: Apple M2, macOS 27.0.1, Apple clang 21.0.0.

### macOS baseline (T005 to T013; starting `src/`, Release, JIT on, vcpkg shared tree, `-Werror`)

Environment: `VCPKG_ROOT=/Users/juvenal/.vcpkg`, `VCPKG_INSTALLED_DIR=/Users/juvenal/.cache/vcpkg-installed/openrender`.

| Task | Result |
|---|---|
| T005 configure (`032-configure`) | clean |
| T006 build (`032-base-build`, `-j4`) | exit 0, no error, every target built; the tree is 469 MB |
| T007 suite run 1 (`032-base-suite`, `-LE determinism-sweep -j4`) | **1097 tests, 100% passed**; 1 test skips itself on the base (`LibShader_Format_rslo_lower_minor`, `Skipped`); 333 s real |
| T007 suite run 2 (`032-base-suite2`) | 1097 tests, 100% passed, same skip; 300 s real; **915 images in each run, 0 differing**; every `MaxBlockAvgDiff` equal (389 listed). `baseline-image-noise-macos.txt` holds only its header: no noise |
| T008 archcheck (`032-base-archreport`) | `ALLOWED DB-004 x 52, DB-006 x 30, DB-010 x 8, DB-011 x 164, DB-017 x 78, DB-025 x 12` (344, `grep -c '^DB-'` = 344); `METRIC Hiders 556/81, Texturing 0/0, TOTAL 604/89`; `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0` |
| T008 shading imports | `nm -m libshader_shading.dylib | grep 'dynamically looked up'`: **empty (0 lines)** |
| T009 | `tools/gi-test-set.txt`: 120 ctest names (derived from `ctest -N -V`: the scene RIBs each test names and the .sl shaders they bind, comments stripped, plus the gather families SC-003 names), including the two `bake3d` sweep scenes marked "sweep only" |
| T010 static facts | see "T010" below |
| T012 frozen copy | `cp -Rp build build-base` (576 MB, `032-freeze-base`); `install_name_tool` + `codesign -f -s -` over **1105 Mach-O files, 217 rewritten, 0 still naming `build/`**; `otool -L build-base/src/orender/orender` names no `build/` path |
| T013 base timing (`032-timing-base`, `build-base` against `build-base`, 5 series, `-t:4`) | see §5 |

First launch of the re-signed `build-base` binaries took 2.7 s once (code-signature validation); later launches do not, so the
first sanity series was discarded.

### T010: the plan's static facts, re-measured on the unmodified tree

No difference from the plan, except as noted.
- 344 `DB-` lines: DB-004 52, DB-006 30, DB-010 8, DB-011 164, DB-017 78, DB-025 12. Line numbers as planned: DB-010 100-107; DB-011
  `irradiance.cpp->renderer.h` 111, `irradianceDispatch.cpp->renderer.h` 112, `worldBmax`/`worldBmin` 125/126, `photonMap.cpp` `toWorld`/`fromWorld` 137/138;
  DB-025 356-367 (356 is the `surface.cpp` line).
- `irradiance.cpp:365-368` front test, `:388` `irradianceSampleAccept` call; `giFunctions.h:235` (`src/libshader/shading/`) and `shading.cpp:3531`
  (`src/libshader/shading/`) `cache->lookup(...)` calls; `photon.cpp:95-103`, the four `tracePhoton` calls at 406/418/557/566, `:630-662`;
  `renderer.cpp:300-303`; `core/map.h:140` `write`, `:247-272` `balance`: all as planned.
- Includers of `irradiance.h`/`photonMap.h`: as planned (`rendererContext.cpp:51,57`, `remoteChannel.cpp:35`, `rendererFiles.cpp:33,36`,
  `rendererStatics.cpp:45,52`, `renderer.cpp:40,47`, `rendererServicesImpl.h:27`, `dataLoad.cpp:8,10`, `irradianceDispatchStub.cpp:46`). The
  Hiders-internal includers are also `irradianceDispatch.cpp:55,57`, `irradiance.cpp:32,34`, `radiance.cpp:44`, `photon.cpp:34`, `photonMap.cpp:33`.
  Tests that include them: M1-M4 and, new here, `tests/gi_split/test_gi_remote_loopback.cpp:49` (`ri/hiders/irradiance.h`, adapted by M5).
- `sizeof(CCacheSample)` 232 with `next` at offset 224; `sizeof(CPhoton)` 44; `sizeof(CPointCloudPoint)` 36 with the `flags` padding at [26, 28)
  (checked by `static_assert`s through the compiler, `-fsyntax-only`).

### Phase 1 tests (approved at PAUSE 1a, 2026-11-09) and the macOS recordings (T020)

Fixture F1, T-A (`GiRemote_LoopbackCache`, `GiRemote_LoopbackPointCloud`), T-F (`GiFiles_Unchanged`), T-B
(`Preview_DataSceneUnchanged`, which adds a `CDataDocument` + recording-sink pass because the `ribdata_*` C API merges batches),
`tests/gi_split/gi_testkit.h`, the tools and the CMake additions were approved as written.

Recording route (maintainer, after PAUSE 1a): the build targets `gi-split-capture-references` (writes a missing reference, refuses
to overwrite one unless configured with `-DOR032_RECAPTURE=ON`) and `gi-split-capture-references-scratch` (the same four into the build
tree), both through `tests/gi_split/capture_references.cmake`, which makes a fresh work directory and renders F1 first. No test binary is
run by hand. The option is named `OR032_RECAPTURE` after `OR037_RECAPTURE`/`OR038_RECAPTURE`.

| Step | Result |
|---|---|
| `032-record-1` (`--target gi-split-capture-references`, from scratch) | exit 0: `tests/gi_split/reference/macos/{files.txt, loopback-cache.bin, loopback-pointcloud.bin}` and `tests/preview/reference/macos/datascene.txt` written |
| `032-record-2` (`--target gi-split-capture-references-scratch`, from scratch) | exit 0 |
| `cmp` of the two recordings | **all four agree byte for byte** (files.txt 2210 B, loopback-cache.bin 48,828 B, loopback-pointcloud.bin 1,588 B, datascene.txt 61,958 B) |
| `032-record-refuse` (the official target again) | exit 2: `exists; refusing to overwrite (configure with -DOR032_RECAPTURE=ON ...)` |
| `ctest -R '^(GiFixture_ICache\|GiFiles_Unchanged\|GiRemote_LoopbackCache\|GiRemote_LoopbackPointCloud\|Preview_DataSceneUnchanged)$'` (`032-p1-ctest-new`) | **5 of 5 passed** on the base |

What the recordings show: the cache fixture holds 93 records (21,752 frame bytes, 232 bytes each); the receiver writes them in 118 nodes.
`numItems` on the point-cloud wire is 12 for 12 stored points; the receiver, built in lookup mode as `getTexture3d()` builds a net client's
cloud, holds 13 points (its constructor balances and stores one root item): recorded as it is (GitHub #131), nothing is corrected.
The photon map of `photon-parity.rib` at `-t:1` holds 46,273 photons and is identical across runs.

### T024 macOS base sweeps

`032-base-sweep` and `032-base-sweep2` (`-L '^determinism-sweep$' -j2`): **419 of 419 passed in both runs** (529 s and 520 s);
419 images in `baseline-sweep-images-macos.sha256`; **0 differing between the runs**, so `baseline-sweep-noise-macos.txt` is empty (header only).
`base-sweep-LastTest-macos.log` and `base-LastTest-macos.log` are force-added (`*.log` is gitignored).

## §1 Architecture reports per checkpoint

Checkpoint 1 (Phase 1, unmodified `src/`): macOS `ctest -L architecture` 29 of 29 passed (`032-p1-final-arch`, with the new tests registered);
full report `ALLOWED DB-004 x 52, DB-006 x 30, DB-010 x 8, DB-011 x 164, DB-017 x 78, DB-025 x 12`, `METRIC TOTAL 604/89`, `RESULT PASS` (identical to
`archcheck-base-macos.txt`). Linux: `archcheck-base-linux.txt` has the same `ALLOWED`/`METRIC`/`RESULT` lines; `ctest -L architecture` 29 of 29 passed.

## §2 Red evidence

### Phase 2, so far (the tests are written; PAUSE 2a and 2b are open, nothing is built red yet)

### T035 to T037 (after PAUSE 2a and 2b, 2026-11-09)

Red evidence, taken in the scratch tree `build-red032` (configured with `-DOPENRENDER_032_RED_TESTS=ON` and all ten sub-checks in
`GI032_BOUNDARY_CHECKS`; only each test's object was built, through `cpujob`; logs `build-logs/032-red-*.log`). Every new test is red
for the stated reason:

| Test (CTest names) | Object target | Result on the starting tree |
|---|---|---|
| T-A2 (`GiRemote_LoopbackLookup`, `GiCache_KindMismatch`) | `tests/gi_split/test_gi_remote_lookup.cpp.o` | `fatal error: 'ri/texture/irradianceCache.h' file not found` (line 42) |
| T-E (`GiKeys_Photons`, `GiKeys_IrradianceRecords`, `GiKeys_PhotonsThreads`) | `tests/gi_split/test_gi_keys.cpp.o` | `fatal error: 'ri/render/photonMap.h' file not found` (line 51) |
| T-C (`TextureOnly_GiStores`) | `red_test_gi_stores` | `fatal error: 'irradianceCache.h' file not found` (line 21; `photonStore.h` is the next in line, Phase 5) |
| T-D (`TextureOnly_IrradianceDrawOrder`) | `red_test_irradiance_draw_order` | `fatal error: 'irradianceCache.h' file not found` (line 31) |
| T-G (`GiSplit_Boundary`) | `ctest -R` in the scratch tree | **Failed**, but only partly verified: the script then lost failures raised inside its functions (fixed in Phase 3; every sub-check is shown red on its own in the Phase 3 table). The counts it printed: friends-texture 3, texture-names-viewer 9, hiders-no-viewer-irradiance 1, friends-hiders 1, texture-names-shading 7, texture-names-core 6, hiders-no-viewer 3, texture-includes 1, hiders-no-texture 6, files-absent 9 |

A fatal include error stops a translation unit at its first missing header, so the other missing members (`lookupCached`, `insertReceived`,
`photonKey`, `CIrradianceRecord::key`, the `getCache` kind check) are not reached; each phase finds the next. Main tree (T036): the
main `build` was reconfigured with the Phase 2 additions (option OFF) and built (`032-p2-green-build`, exit 0, nothing from `src/`
changed); `ctest -L 'gi_split|preview'` passed; `ctest -L architecture` passed (30 of 30, `GiSplit_Boundary` green with no sub-check
enabled). T035's cleanup is done: the scratch tree `build-red032` was deleted by the coordinator on the maintainer's word (2026-11-09).

T031: `GiSplit_Boundary` run on the unmodified tree with every sub-check enabled (`032-p2-boundary`, `cmake -P` of
`tests/architecture/check_gi_split_boundary.cmake`, include directories of `ribRender_obj`). What matches today, which is the red
state each phase turns green. **Only partly verified at the time:** the script then lost the failures raised inside two of its functions (see Phase 3), so of the ten only `files-absent` was really shown to fail; the other nine were verified in Phase 3 (table below):

```
  friends-texture: src/ri/texture/brickmap.h: friend class CBrickMapGeometry;
  friends-texture: src/ri/texture/pointCloud.h: friend class CRemotePtCloudChannel;
  friends-texture: src/ri/texture/texture3d.h: friend class CRemotePtCloudChannel;
FAIL: friends-texture: 3 match(es)
  texture-names-viewer: src/ri/texture/brickmap.h: class CBrickMapGeometry;
  texture-names-viewer: src/ri/texture/brickmap.h: friend class CBrickMapGeometry;
  texture-names-viewer: src/ri/texture/pointCloud.h: // CDataView interface for drawing
  texture-names-viewer: src/ri/texture/pointCloud.h: friend class CRemotePtCloudChannel;
  texture-names-viewer: src/ri/texture/pointHierarchy.h: // CDataView interface
  texture-names-viewer: src/ri/texture/texture3d.h: #include "dataView.h"
  texture-names-viewer: src/ri/texture/texture3d.h: class CTexture3d : public CFileResource, public CDataView {
  texture-names-viewer: src/ri/texture/texture3d.h: // CDataView channel accessors (FR-016) — shared by every CTexture3d-derived document
  texture-names-viewer: src/ri/texture/texture3d.h: friend class CRemotePtCloudChannel;
FAIL: texture-names-viewer: 9 match(es)
  hiders-no-viewer-irradiance: src/ri/hiders/irradiance.cpp: #include "debug.h"
FAIL: hiders-no-viewer-irradiance: 1 match(es)
  friends-hiders: src/ri/hiders/irradiance.h: friend class CRemoteICacheChannel;
FAIL: friends-hiders: 1 match(es)
  texture-names-shading: src/ri/texture/brickmap.h: void lookup(float *, const float *, const float *, const float *, const float *, CShadingContext *) { assert(FALSE); }
  texture-names-shading: src/ri/texture/pointCloud.h: void lookup(float *, const float *, const float *, const float *, const float *, CShadingContext *) { assert(FALSE); }
  texture-names-shading: src/ri/texture/pointHierarchy.cpp: void CPointHierarchy::lookup(float *Cl, const float *Pl, const float *, const float *, const float *Nl, CShadingContext *context) {
  texture-names-shading: src/ri/texture/pointHierarchy.h: float (*maxSolidAngle)(const CShadingContext *context);
  texture-names-shading: src/ri/texture/pointHierarchy.h: void lookup(float *, const float *, const float *, const float *, const float *, CShadingContext *);
  texture-names-shading: src/ri/texture/texture3d.h: class CShadingContext;
  texture-names-shading: src/ri/texture/texture3d.h: virtual void lookup(float *, const float *, const float *, const float *, const float *, CShadingContext *) = 0;
FAIL: texture-names-shading: 7 match(es)
  texture-names-core: src/ri/texture/texture.cpp: // Description			:	Set the maximum texture memory and init the cache (was CRenderer::initTextures)
  texture-names-core: src/ri/texture/texture.cpp: // Description			:	Free the counters (was CRenderer::shutdownTextures)
  texture-names-core: src/ri/texture/textureInternal.h: *   Texturing-private (spec 027): the tile cache that texture.cpp used to keep in CRenderer statics,
  texture-names-core: src/ri/texture/textureInternal.h: TMutex mutex;                 // guards the registry during eviction (was CRenderer::textureMutex)
  texture-names-core: src/ri/texture/textureResolver.h: // relative to the working directory) when the file exists. This is what CRenderer::locateFile does
  texture-names-core: src/ri/texture/textureSystem.h: *   was given. The renderer core constructs one per frame (CRenderer::textureSystem) and Shading
FAIL: texture-names-core: 6 match(es)
  hiders-no-viewer: src/ri/hiders/irradiance.cpp: #include "debug.h"
  hiders-no-viewer: src/ri/hiders/photonMap.h: #include "dataView.h"
  hiders-no-viewer: src/ri/hiders/photonMap.h: class CPhotonMap : public CMap<CPhoton>, public CFileResource, public CDataView, public CMapRefTarget, public openrender::shading::IPhotonLookup {
FAIL: hiders-no-viewer: 3 match(es)
  texture-includes: src/ri/texture/texture3d.h includes src/ri/dataviewer/dataView.h (Consumer)
FAIL: texture-includes: 1 include(s)
  hiders-no-texture: src/ri/hiders/irradiance.cpp includes src/ri/texture/pointHierarchy.h (Texturing)
  hiders-no-texture: src/ri/hiders/irradiance.cpp includes src/ri/texture/texture.h (Texturing)
  hiders-no-texture: src/ri/hiders/irradiance.h includes src/ri/texture/texture3d.h (Texturing)
  hiders-no-texture: src/ri/hiders/irradianceDispatch.cpp includes src/ri/texture/pointHierarchy.h (Texturing)
  hiders-no-texture: src/ri/hiders/irradianceDispatch.cpp includes src/ri/texture/texture.h (Texturing)
  hiders-no-texture: src/ri/hiders/radiance.cpp includes src/ri/texture/texture.h (Texturing)
FAIL: hiders-no-texture: 6 include(s)
  files-absent: src/ri/hiders/harmonics.h exists
  files-absent: src/ri/hiders/irradianceDispatch.cpp exists
  files-absent: src/ri/hiders/radiance.cpp exists
  files-absent: src/ri/hiders/radiance.h exists
  files-absent: src/ri/vector/irradianceDispatchStub.cpp exists
  files-absent: src/ri/hiders/irradiance.cpp names irradianceSampleAccept in code
  files-absent: src/ri/hiders/irradiance.h names irradianceSampleAccept in code
  files-absent: src/ri/hiders/irradianceDispatch.cpp names irradianceSampleAccept in code
  files-absent: src/ri/vector/irradianceDispatchStub.cpp names irradianceSampleAccept in code
FAIL: files-absent: 9 finding(s)
10 sub-check(s) enabled
  GiSplit_Boundary: 1 failure(s).
  tests/architecture/check_gi_split_boundary.cmake:213 (or_check_end)
```

`texture-names-core` matches exactly the six comments the tasks predict (`texture.cpp:3073,3101`, `textureInternal.h:7,54`,
`textureResolver.h:48`, `textureSystem.h:10`). `texture-includes` finds one include of another domain
(`texture3d.h -> dataviewer/dataView.h`, class Consumer). `files-absent` finds the five files and four code mentions of
`irradianceSampleAccept` (`irradiance.cpp`, `irradiance.h`, `irradianceDispatch.cpp`, `irradianceDispatchStub.cpp`).

The registration of `GiSplit_Boundary` (label `architecture`, `GI032_BOUNDARY_CHECKS` empty) was configured in the main `build`:
`ctest -L architecture` **30 of 30 passed** (the 29 of Phase 1 plus `GiSplit_Boundary`, a pass with no sub-check enabled).

## §3 Per-phase checkpoint records

### Phase 1, Linux (T021 to T025; Ubuntu 24.04, GCC 13, `-Werror`, remote `cpujob` at `-j2`, tree `~/Projects/Development/CLI/openrender-032`)

The host was unreachable at the first report and came back; the tree was checked absent before the first push, and no other tree was touched.
Remote disk 9.2 GB free before, 8.7 GB after `build-base`.

| Task | Result |
|---|---|
| T021 `linux_sync.sh push` / `verify` | pushed 3,744 files, **0 differences**; later pushes 3,751 and 3,755; final `verify`: 3,755 files, 0 differences; no remote `.git` |
| T022 configure + build (`032-l-configure`, `032-l-base-build`, Release, JIT on, LLVM 18, `CPUJOB_TIMEOUT=7200` set by the script) | exit 0, no warning under `-Werror`, every target built (the three new test targets included) |
| T022 suite 1 and 2 (`-LE determinism-sweep -E <the five new tests> -j2`) | **1098 tests, 1097 passed, 1 failed in both runs**: `TextureTile_RenderByteIdentical` (the baseline failure #45, listed in spec 029's Linux baseline too); 1 self-skip (`LibShader_Format_rslo_lower_minor`); 711 s and 676 s. **915 images, 0 differing between runs**; the `MaxBlockAvgDiff` listings are identical; `baseline-image-noise-linux.txt` is empty (header only) |
| T022 archcheck | `ALLOWED` as macOS (344), `METRIC TOTAL 604/89`, `RESULT PASS`; `ctest -L architecture` 29 of 29 |
| T022 imports | `nm -D --undefined-only libshader_shading.so | c++filt`: **461 undefined dynamic symbols** (`libshader-imports-base-linux.txt`; Linux links with `--no-undefined`, so these are libc/libstdc++/libRIB* symbols) |
| T023 recordings (`gi-split-capture-references`, then `-scratch`, from scratch) | both exit 0; **all four references agree byte for byte** (`files.txt` 2,223 B, `loopback-cache.bin` 48,828 B, `loopback-pointcloud.bin` 1,588 B, `datascene.txt` 61,983 B). They differ from macOS in `files.txt`, `loopback-cache.bin` and `datascene.txt` (float results and sample order differ by platform: per-platform references) and equal macOS in `loopback-pointcloud.bin` |
| T023 `ctest -R <the five new tests>` on Linux | **5 of 5 passed** |
| T023 frozen `build-base` | `cp -a build build-base` (remote `cpujob`); `patchelf` is not installed and the binaries carry `RUNPATH` naming `build/`, so `timing_sc007.sh` puts the copy's library directories first in `LD_LIBRARY_PATH` on Linux (searched before `RUNPATH`) |
| T023 base timing (`timing_sc007.sh build-base build-base 5`, `-t:4`; `SCENE_THREADS="perf-ptc=1"`) | see §5. A first attempt at `-t:4` for `perf-ptc` died with `orender failed (-11)` (segmentation fault in `bake3d`, GitHub #122, FR-030): not judged, the scene is pinned to one thread on Linux (`timing-base-linux.txt`; the first attempt's output is not kept) |
| T024 sweeps (`-L '^determinism-sweep$' -j2`) | **419 of 419 passed in both runs** (1,392 s and 1,341 s); 419 images; **0 differing between runs**; `baseline-sweep-noise-linux.txt` is empty. No `bake3d` crash in the sweeps |
| T025 `linux_sync.sh pull` / `verify` | every `-linux` file named in T022 to T024 exists locally; the Linux references are in `tests/gi_split/reference/linux/` and `tests/preview/reference/linux/datascene.txt`; `verify`: 3,755 files (18 pulled), 0 differences |

## §4 Images and sweeps

## §5 SC-007 timing

Base against base (`032-timing-base`, `timing_sc007.sh build-base build-base 5`, THREADS 4, scales `perf-occlusion-cache` format x12
shadingrate x3, `perf-indirect-photon` format x6 shadingrate x3 emit x10, `perf-ptc` format x6 shadingrate x3), medians and spreads in
seconds, saved as `tools/timing-base-macos.txt`:

| Scene | start median (spread) | final median (spread) |
|---|---|---|
| perf-occlusion-cache | 7.31 (4.04) | 6.00 (3.07) |
| perf-indirect-photon | 6.21 (3.84) | 6.89 (7.28) |
| perf-ptc | 5.01 (3.66) | 4.98 (3.28) |

Linux (`timing-base-linux.txt`, 5 series, `-t:4`, `perf-ptc` at `-t:1`):

| Scene | start median (spread) | final median (spread) |
|---|---|---|
| perf-occlusion-cache | 9.22 (0.32) | 9.19 (0.31) |
| perf-indirect-photon | 13.63 (0.35) | 13.53 (0.21) |
| perf-ptc | 6.82 (0.37) | 6.87 (0.28) |

No scene under 3 s on either platform. The macOS spreads are large because the machine was shared with other specs' builds and suites for the whole run
(the `cpujob` lock serialises jobs of the same user but not their render load or the thermal state); the bar of SC-007 is this spread.
The unscaled templates render in 0.05 to 0.1 s, so the scales above were chosen from a sanity series (the first scales gave 1.1 s for
`perf-occlusion-cache`, which was raised from x6 to x12).

## §6 Departures and findings

### Phase 2

- **Retraction (Phase 1 report).** The Phase 1 report listed "`orender` exits 0 when a render fails early" as a finding. It has no repro
  and is withdrawn: the sub-second timings that suggested it were real, fast renders (valid `.tif` outputs, exit 0), and the one real
  failure seen (`orender failed (-11)`, #122) was caught by its nonzero return code. It was never recorded in this file.
- **T-E's seam (for the record-key test).** The renderer creates and frees the irradiance cache inside the frame, so a test
  cannot read it afterwards, and no key is written to a cache file (Q3 B). `test_gi_keys.cpp` therefore installs a
  `CRendererServices` wrapper over the renderer's own for the length of the frame (a substitute installed after `RiBegin` survives
  `RiWorldBegin`, gotcha 23); its `getCache()` builds a `CIrradianceCache` subclass whose `insertRecord()` copies every
  `(key, record)` first. Nothing in `src/` changes for it. After PAUSE 2a the wrapper derives from `CContractForwarder`
  (`tests/shading_contract/contract_forwarder.h`) and overrides `getCache()` only, so it lists no member of `CRendererServices`.
  `CContractForwarder` is `final`, so this needs the one-word edit M7 (`tools/test-edit-patches/M7-contract_forwarder.diff`), not applied
  yet; **approved by the maintainer (2026-11-09)**, it lands exactly as in the patch with T-E's enabling in Phase 7 (T108a). `CIrradianceCache` and `insertRecord` must stay non-final.
  The photon half needs no seam: it keeps the map alive with `attach()` and reads `photonKey(i)` after the frame.
- **T-A2 shares no code with T-A.** Its loopback helpers are in a new header `tests/gi_split/gi_loopback.h`, because T-A (approved,
  recorded) is not edited.
- **T-C and T-D definitions.** In the red block they are OBJECT libraries on the narrow include path (the real library and
  executables come with M6a, whose sources do not exist yet); T-A2 and T-E are real executables and tests in the block.
- **T-D's tree.** The root of its cache is 0.1 wide, so no record can descend and the lookup always visits every record in list order;
  the expectation is computed from `forEachRecord()` (the cache's own order, after `clamp()`).

- **T032 line numbers (re-grepped 2026-11-10, Phase 4 start).** M1 `test_photon_map_frame_release.cpp:44`, M2 `test_photon_map_sessions.cpp:45`,
  M3 `test_data_world_init.cpp:7`, M4 `test_dataview_chunking.cpp:5` (each `#include "ri/hiders/photonMap.h"`), M5 `test_gi_remote_loopback.cpp:49` and
  `:260-261`, M6a at the end of the 59-line `tests/unit/texture_only/CMakeLists.txt`; M1-M5, M6a and M7 pass `git apply --check` on this tree (M6b applies only
  after M6a, Phase 5). Tasks T027 to T037 were ticked in `tasks.md` on 2026-11-10 (housekeeping): T027 to T031 by the files and the red table above, T032 by
  the eight patches and this line record, T033/T034 by the red-evidence heading "after PAUSE 2a and 2b" and the approvals recorded in this section (M7 approved
  2026-11-09), T035 to T037 by the table above and the Phase 2 commit `1cc0416c`.

### Phase 4

- Out-of-spec defects found while moving the cache, filed by the maintainer and **not fixed here**: GitHub #158 (the JIT's `jitOcclusionBatch` returns without writing its output when `getCache` answers
  `nullptr`, unchanged from the starting tree) and #159 (`getTexture3d` returns a registered irradiance cache, a wrong-kind frame file, cast to `CPointCloud *`), both P1.
- `transformBound` lives in Graphics State: the cache computes the world box with a file-local copy (`transformWorldBound`), which keeps Texturing free of State and the Texturing-only link line closed.

## §7 SC-001 to SC-010 walk

### Phase 3 (T038 to T053): the 3D family's ties and the Texturing-derived adapters

**Test gate (T038).** Nine allowlist lines deleted by key (DB-025: `texture3d.h -> dataView.h`; `brickmap.cpp` -> `drawPoints`, `drawTriangles`,
`drawDisks`; `pointCloud.cpp` -> `drawPoints`, `drawDisks`; `irradiance.cpp` -> `debug.h`, `drawPoints`, `drawDisks`); the DB-025 section header
`(12)` is `(3)` (`surface.cpp`, `photonMap.h`, `photonMap.cpp` remain); the allowlist is **344 -> 335**. `friends-texture`,
`texture-names-viewer` and `hiders-no-viewer-irradiance` are the default of `GI032_BOUNDARY_CHECKS` (`tests/gi_split/CMakeLists.txt`).
Red, taken on the unmodified `src/` (the Phase 3 source edits were stashed for the run and restored; `032-p3-red-arch.log`):
`Architecture_DomainBoundaries` fails with **exactly the nine `VIOLATION` lines of the deleted keys** and nothing else (`violations=9`).
`GiSplit_Boundary` against a `git archive HEAD` copy of `src/` (`032-p3-boundary-red.log`): friends-texture 3, texture-names-viewer 9,
hiders-no-viewer-irradiance 1, exit 1.

**A defect in T-G, found and fixed here (needs the maintainer's ratification).** The first red run showed `GiSplit_Boundary` *passing* on the
unmodified tree. Cause: `gi_scan()` and `gi_include_classes()` are CMake functions and `or_check_fail()` is a macro that counts in the scope it
is expanded in, so every failure raised inside them was lost on return (the Phase 2 evidence did not show it because `files-absent` is called at
the top level and failed alone). Fix, in the approved `tests/architecture/check_gi_split_boundary.cmake`: a `gi_fail()` function that records the
failure in a global property, counted before `or_check_end()`. Re-run on the same pristine copy: the three Phase 3 sub-checks fail and the script
exits 1. The diff is the file's last staged change.

**Implementation** (`git diff --cached --stat -- src`: 18 files, +930 / -507). Texturing:
`texture3d.h` loses `dataView.h`, the `CDataView` base and the `CRemotePtCloudChannel` friend, and gains public `defineChannels(const char *)`,
`setTransform`, `setDPScale`, `channelSampleStart`, `channelNumSamples`; `pointCloud.{h,cpp}` lose `draw`/`keyDown`/`currentChannel`/`drawMode`,
the two statics and the friend, and gain `points()`, `pointData()` (`getNumPoints()` became `const`); `brickmap.{h,cpp}` lose the forward
declaration and friend of `CBrickMapGeometry`, `draw`/`keyDown` and the three statics, and gain `visitVoxels`, `maxLevel`, `sideLength`;
`pointHierarchy.h` loses the three empty viewer members. `pointCloud.cpp` and `brickmap.cpp` have **deletions only** (no line added, so no body
of `store`, the lookups, the hierarchy traversal or the brick paging changed). The six-argument `lookup(..., CShadingContext *)` and
`CPointHierarchy::Options` stay until Phase 4 (D1). Hiders: `irradiance.{h,cpp}` lose `draw`, `keyDown`, `drawDiscs`, `debug.h`; they gain
`forEachNode`, `forEachRecord`, `rootCenter`, `rootSide`, `setRoot`, `insertReceived` and `treeDepth()` (**named `treeDepth`, not `maxDepth`,
because the private member of that name exists**). Consumer: new `src/ri/dataviewer/{pointCloudView,brickMapView,irradianceCacheView}.{h,cpp}`
with the `Document` classes, built by `dataLoad.cpp`; `dataLoad.h` now includes `dataView.h` (it used to forward-declare `CDataView`, which
`test_data_pointcloud.cpp` needs complete for its `dynamic_cast`; it had received it through `texture3d.h`). Core: the point-cloud remote channel
reads through `getNumPoints()`, `points()`, `pointData()`, `numChannels()`, `channelName()`, `setTransform`, `setDPScale`; the sent bytes are unchanged.
`src/ri/CMakeLists.txt` lists the three adapters (they reach `ribVector_sources`).

**macOS checkpoint** (all `cpujob`): build `-j4` exit 0 under `-Werror`; the T050 selection
(`Preview_DataSceneUnchanged|test_data_|test_dataview_chunking|test_debugdump_roundtrip|test_wire_cli|GiRemote_Loopback|GiFiles_Unchanged|TextureOnly_|PluginAbi|Texture3dChannels|Photon|photonmap`)
**40 of 40 passed**, `GiRemote_LoopbackCache`, `GiRemote_LoopbackPointCloud` and `Preview_DataSceneUnchanged` against their base recordings; the full suite
`-LE determinism-sweep` **1103 of 1103 passed** (1097 base + the 5 Phase 1 tests + `GiSplit_Boundary`); images: 915 in the manifest, **0 differing, 0 missing**,
4 not in the manifest (the new tests' own images), every `MaxBlockAvgDiff` listing identical to the base; the GI set is part of that run; `libshader_shading`
dynamically looked-up imports: 0 (as the base); `ctest -L architecture` 30 of 30; the full report `ALLOWED ... DB-025 x 3`, `METRIC TOTAL 604/89`, `RESULT PASS`.
`grep -rn "friend " src/ri/texture src/ri/hiders` shows `makeBrickMap` and `CRemoteICacheChannel` only (the latter goes in Phase 4).

**Linux checkpoint (T052; GCC 13 `-Werror`, remote `cpujob` at `-j2`, `openrender-032`).** Build exit 0 (no warning, `-Woverloaded-virtual` clean); the T050
selection **40 of 40 passed**; the full suite `-LE determinism-sweep`: **1104 tests, 1102 passed**; the two failures were `TextureTile_RenderByteIdentical` (the
baseline failure #45) and `GiSplit_Boundary`. The second was a second defect of T-G, found only on Linux: a `cmake -P` script starts with the old policies, so on
CMake 3.28 `IN_LIST` (CMP0057) was read as a plain argument and the script errored even with no sub-check enabled. Fix (staged, same file as the macOS fix):
`cmake_minimum_required(VERSION 3.19)` at the top of `check_gi_split_boundary.cmake`. After pushing it and configuring the remote tree with
`-DGI032_BOUNDARY_CHECKS=<the Phase 3 list>`, `ctest -L architecture` is **30 of 30** and the full report is `ALLOWED ... DB-025 x 3`, `RESULT PASS`.
Images: 915 in the manifest, **0 differing, 0 missing** (4 new images of the new tests); the `MaxBlockAvgDiff` listing is identical to the base; the dynamically
resolved undefined symbols of `libshader_shading.so` are the same 461 as the base. (A tree configured before Phase 3 keeps its cached, empty
`GI032_BOUNDARY_CHECKS`: configure with `-DGI032_BOUNDARY_CHECKS="friends-texture;texture-names-viewer;hiders-no-viewer-irradiance"`.)

**Other notes.** The two edits to the approved `check_gi_split_boundary.cmake` (the lost failures, the policy) are defect fixes, not changes of what it checks; both
are in the staged diff for ratification. `contract_forwarder.h` and M1 to M6 are not touched (their phases are later). Two shared-scratchpad and shared-stash
facts worth knowing: `git stash` is shared across worktrees (the run used and dropped `stash@{0}` by message), and the pristine `src/` copy used for the red run
is `scratchpad/032-pristine` (untracked, outside the repo).

**Each of the ten T-G sub-checks, red on its own (maintainer's request after Phase 3).** The fixed `check_gi_split_boundary.cmake`, once per sub-check
(`CHECKS=<that one>`), through `cpujob`, against a `git archive 1cc0416c src` copy (the Phase 2 commit's `src/`) under `build-logs/`, removed afterwards
(logs `build-logs/032-p3-each-<check>.log`):

| Sub-check | Exit | Matches (what they are) |
|---|---|---|
| friends-texture | 1 | 3 (`brickmap.h` `CBrickMapGeometry`; `pointCloud.h`, `texture3d.h` `CRemotePtCloudChannel`) |
| texture-names-viewer | 1 | 9 (`CBrickMapGeometry`, `CDataView`, `CRemote*`, `dataView.h` in `brickmap.h`, `pointCloud.h`, `pointHierarchy.h`, `texture3d.h`) |
| hiders-no-viewer-irradiance | 1 | 1 (`irradiance.cpp` `#include "debug.h"`) |
| friends-hiders | 1 | 1 (`irradiance.h` `friend class CRemoteICacheChannel`) |
| texture-names-shading | 1 | 7 (`CShadingContext` in `brickmap.h`, `pointCloud.h`, `pointHierarchy.{h,cpp}`, `texture3d.h`) |
| texture-names-core | 1 | 6 (the six historical comments naming `CRenderer`) |
| texture-includes | 1 | 1 (`texture3d.h -> dataviewer/dataView.h`, class Consumer) |
| hiders-no-texture | 1 | 6 (`irradiance.{h,cpp}`, `irradianceDispatch.cpp`, `radiance.cpp` include Texturing headers) |
| files-absent | 1 | 9 (the five files, four code mentions of `irradianceSampleAccept`) |
| hiders-no-viewer | 1 | 3 (`irradiance.cpp` `debug.h`; `photonMap.h` `dataView.h` and `CDataView`) |

Every sub-check exits non-zero for its stated reason. The three Phase 3 ones are green on the Phase 3 tree; the other seven stay red until their phases.

**Approved after the Phase 3 report** (also in plan.md and tasks.md D17 to D19): `treeDepth()` for the cache's depth accessor, `dataLoad.h` including `dataView.h`, and
`const getNumPoints()`; the two `check_gi_split_boundary.cmake` fixes were ratified after the fact.

### Phase 4 (T054 to T077): irradiance cache split

**PAUSE 4a (T054), approved by the maintainer 2026-11-10.** M5 and M6a approved exactly as shown (`tools/test-edit-patches/M5-test_gi_remote_loopback.diff`,
`M6a-texture_only-CMakeLists.diff`); the M5 include stays where the patch puts it (not alphabetised). Both pass `git apply --check` at the start of Phase 4.
Any difference between the applied diff and the approved one stops the work. Housekeeping done at the same pause: Phase 1 tasks T001 to T026 ticked
(all confirmed), the duplicate `§5` heading merged.

**Test gate (T055).** Twelve allowlist lines deleted by key: all eight DB-010 lines with their section header (`irradiance.cpp -> pointHierarchy.h`, `-> texture.h`;
`irradiance.h -> texture3d.h`; `irradianceDispatch.cpp -> pointHierarchy.h`, `-> texture.h`; `irradiance.cpp ->` the `CTexture3d` constructor, destructor and typeinfo);
DB-011 `irradiance.cpp -> renderer.h`, `irradianceDispatch.cpp -> renderer.h`, `irradiance.cpp -> CRenderer::worldBmax` and `-> worldBmin`; the DB-011 header `(164)`
is `(160)`; the allowlist is **335 -> 323** (`grep -c '^DB-'`). The six Phase 4 sub-checks (`friends-hiders`, `texture-names-shading`, `texture-names-core`,
`texture-includes`, `hiders-no-texture`, `files-absent`) are added to the default of `GI032_BOUNDARY_CHECKS` (`tests/gi_split/CMakeLists.txt`). Red, on the Phase 3
tree (`032-p4-red-configure`, `032-p4-red-arch`, `build-logs/032-p4-red-arch.log`): `Architecture_DomainBoundaries` fails with **exactly the twelve `VIOLATION`
lines of the deleted keys** (eight at `irradiance.cpp`/`irradiance.h`, four at `irradianceDispatch.cpp`: the DB-010 and DB-011 include and link edges) and nothing else;
`GiSplit_Boundary` fails on `friends-hiders` 1, `texture-names-shading` 7, `texture-names-core` 6, `hiders-no-texture` 6, `files-absent` 9 (`texture-includes` has nothing
to match: the Phase 3 edit already removed its one include).

**Implementation (T056 to T070).** Texturing: `irradianceStore.h` (new, header-only: the `CACHE_*` flags, `CIrradianceRecord`, `IrradianceDrawFn`, `CIrradianceStore`,
`irradianceStoreOf`); `hiders/irradiance.{h,cpp}` moved with `git mv` to `texture/irradianceCache.{h,cpp}` and converted (derives from `CIrradianceStore`; the
constructor takes `worldBmin/worldBmax`; `lookupCached` is the old `lookup` up to the miss decision with the two substitutions; `insertRecord` is the old insertion
under the lock; `sample()`, `lookup(..., CShadingContext *)` and `friend CRemoteICacheChannel` are gone). `texture3d.h` loses `CShadingContext` and the six-argument `lookup`
(now `lookupPointBased` with `CPointBasedLookup`), and `pointHierarchy.{h,cpp}` loses `Options`; under `src/ri/texture` the quickstart section 1 scan finds nothing.
Shading: `irradianceSampler.h` (`IIrradianceSampler`, `IrradianceSample`), `irradianceCalls.{h,cpp}` (`libshader::irradianceLookup`), the sampler slots in `shading.{h,cpp}`
(`setIrradianceSampler`, `setDefaultIrradianceSampler`), the two call sites (`giFunctions.h` `IDEXPR`, `shading.cpp` `jitOcclusionBatch`). Hiders: `irradianceDispatch.cpp` moved
to `irradianceSampler.cpp` (`CIrradianceSampler::sample`, the gradients and the radius now computed outside the lock) with `hiderIrradianceSampler.h`; `hider.cpp` installs it on
every hider's context, `rendererShadingServices.cpp` as the process default. Core: `rendererFiles.cpp` (constructor arguments, `getCache` answers
`dynamic_cast<CIrradianceStore *>`, `pointHierarchyMaxSolidAngle` gone), `remoteChannel.cpp` (the cache channel goes through `forEachNode`, `insertReceived`, `rootCenter`,
`rootSide`, `setRoot`; the sent bytes are unchanged), include paths. Viewer: `irradianceCacheView.{h,cpp}`, `dataLoad.cpp` pass the world bound. Deleted:
`vector/irradianceDispatchStub.cpp`, `hiders/radiance.{h,cpp}`, `hiders/harmonics.h` (compiled by no target, as OQ-4 said). `src/ri/CMakeLists.txt`, the residual list
(`irradianceDispatchStub.cpp` row deleted, `rendererStatics.cpp` cell updated), M5 and M6a (applied exactly as approved; `git apply --check` passed on both) and T-A2/T-C/T-D enabled.
Two deviations of detail from the text, both inside the spec: (1) **`transformBound` is Graphics State's** (`state/xform.cpp`), which Texturing may not include and the
Texturing-only link line (T-C/T-D) does not have, so the cache computes the world box with a file-local copy of the same corner order and arithmetic (`transformWorldBound`);
(2) `forEachRecord` passes a `CIrradianceRecord` copy (the approved tests `static_cast` to it, and `CCacheSample` must stay standard-layout for the approved `offsetof(next)` of T-A),
so `CCacheSample` has `get()`/`set()` and the cache keeps its raw-bytes wire layout. The interpreter's `assert(cache->dataSize == 7)` became `assert(cache == NULL || ...)` so a
null cache reaches `irradianceLookup` in a debug build too.

**T065, null cache.** `getCache` answers `nullptr` for a name registered as a point cloud or brick map (`GiCache_KindMismatch` passes: a different kind answers `nullptr`, and the
cache obtained under another name is returned unchanged by a second call). The interpreter's `IDEXPR_PRE` reaches `irradianceLookup` with the null cache (`rendererTexture3dResolve`
tests its argument; the `dataSize` assert was made null-tolerant), which writes the no-joy value. The JIT's `jitOcclusionBatch` returns before the lookup when `getCache` answers
`nullptr` (`if (!cache) return;`, unchanged from the base), leaving the destination as it was. The remote-server path `CHANNELTYPE_ICACHE` (`remoteChannel.cpp`) casts `getCache`'s answer
with a C cast and would hand a null to `CRemoteICacheChannel`'s constructor for a wrong-kind name (the base handed it the wrong object): reported as a finding, not changed.

**T070 stale references** (`grep -rn "irradiance\.h\|irradianceDispatch\|irradianceSampleAccept\|radiance\.h\|harmonics\.h"` over `src tests CMakeLists.txt`): comments only, in files this
spec may not edit or fixes in Phase 6: `src/ri/hiders/depository.cpp:24`, `src/ri/render/rendererFileSearch.cpp:22`, `src/preview/libribpreview/dataScene.h:14`,
`tests/visual/CMakeLists.txt:657`, `tests/preview/test_data_world_init.cpp:85`; plus the descriptions inside `tests/architecture/check_gi_split_boundary.cmake` and the basename guard
of `tests/link/check_scope.cmake` S1b (`irradiance.h`), which is meant to name the old header. The six historical comments under `src/ri/texture/` that named `CRenderer` are reworded.

**macOS checkpoint** (all through `cpujob`): CP-a build `-j4` exit 0 under `-Werror` after one fix (`CMemStack` needs `common/containers.h`); a no-JIT configure and build of
`libshader_shading` and `ribRender` (`032-p4-nojit`) exit 0, `build-nojit` deleted. `nm -u` of `hiders/irradianceSampler.cpp.o`: 15 undefined symbols, none a `CIrradiance*`,
`CTexture3d` or `CPointHierarchy` one (`CPhotonMap::lookup`, `CTextureLookup::staticInit`, `CShadingContext::lookupEnvironment`, `libshader::traceRay`, the sampling helpers and libc/libc++).
`nm -m libshader_shading.dylib | grep 'dynamically looked up'`: **0 lines, equal to the base** (`tools/libshader-imports-after-phase4-macos.txt`, empty). `ctest -L link` **4 of 4**; the
shading-contract, trace-replay and shading tests (`-R 'ShadingContract_|ShadingTrace|TraceReplay|Shading.*Replay'`) **66 of 66**, unedited. The new tests: `GiFixture_ICache`, `GiFiles_Unchanged`,
`GiRemote_LoopbackCache`, `GiRemote_LoopbackPointCloud`, `Preview_DataSceneUnchanged`, `GiCache_KindMismatch`, `TextureOnly_Standalone` pass against their base recordings;
**three enabled tests fail, each for a reason in the test, none in the cache** (see "Findings" below): `GiRemote_LoopbackLookup`, `TextureOnly_GiStores`, `TextureOnly_IrradianceDrawOrder`.
Full suite `-LE determinism-sweep`: **1107 tests, 1104 passed, the three above failed**; no other test failed. CP-d: 920 images in the manifest, of which the 915 of the base
are **0 differing, 0 missing** (the five more are the new tests' own images); the `MaxBlockAvgDiff` listing equals the base (`diff` empty), the GI test set included, `-slo` variants too.
CP-c: `ctest -L architecture` **30 of 30**; the full report `ALLOWED DB-004 x 52, DB-006 x 30, DB-011 x 160, DB-017 x 78, DB-025 x 3` (DB-010 gone), `METRIC Hiders 553/80, Texturing 0/0, TOTAL 601/88`,
`RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; `GiSplit_Boundary` green with the nine sub-checks of Phases 3 and 4; allowlist **335 -> 323**.
Informational SC-007 (`timing_sc007.sh build-base build 3`, `-t:4`): perf-occlusion-cache -3.4%, perf-indirect-photon +2.4%, perf-ptc +8.0% (spread 0.83 s), all within the spread, none beyond; no scene under 3 s.

**Findings (Phase 4).** The three failing tests were written in Phase 2 against an implementation that did not exist and were first run here. Each fails for a reason the
unchanged base behaviour explains, so none can be fixed in `src/` without changing images; each needs an edit of an approved test, which waits for approval:
1. `TextureOnly_IrradianceDrawOrder`: its draw count and its draw order pass; the blend fails on `C[6]` (the envdir z) at 0.99927 against the expected 1.0 (relative error 7e-4 > the 1e-4 it allows).
   The cache normalizes the envdir with `normalizevf`, which uses the approximate `isqrtf`, as the base did, and the images depend on it. Proposed edit: compute the expected envdir with `normalizevf` itself.
2. `TextureOnly_GiStores`: the cache is written with `CACHE_WRITE | CACHE_SAMPLE` and read back with `CACHE_READ | CACHE_RDONLY`, and the lookup's discard threshold (`smallSampleWeight`) is 0.1 with the
   first flag set and 0 without: a record of weight below 0.05 counts only after the reload (irradiance 0.0946554 before, 0.0961394 after at the first query). The reader also lists each node's records in
   the reverse of the order they were written, so even with equal flags the sums differ in the last bits (checked with a scratch copy of the test in the scratchpad, `032-dbg-stores2.cpp`, which gives the reader `CACHE_SAMPLE` too: the difference is then one ulp, 0.0946554467 against 0.0946554393). Proposed edit: the same `CACHE_SAMPLE` flag on both sides and a 1e-5 relative bound on `C` (`Pc`, `Nc` stay exact).
3. `GiRemote_LoopbackLookup` (T-A2): it fails only when the draw counts differ (sender 707, receiver 628) while it reports 12 of 93 positions that differ and hits 93/93 on both sides. The receiver places a record by
   `dP` without the sender's `1/K` (OQ-5), so its tree is deeper and the lookup visits fewer nodes: a different draw count is the same difference seen through the callback. Proposed edit: report the draw counts like the
   positions and fail on the hit counts only.

**Linux checkpoint (T076; Ubuntu 24.04, GCC 13 `-Werror`, remote `cpujob` at `-j2`, `openrender-032`; the host had rebooted and was reachable).** Pushed 3,777 files, `verify` 0 differences;
configure with the Phase 4 `GI032_BOUNDARY_CHECKS` list; build exit 0 with no warning and no error (`-Woverloaded-virtual` clean); the full suite `-LE determinism-sweep`: **1108 tests, 1104 passed, 4 failed**:
`TextureTile_RenderByteIdentical` (the baseline failure #45) and the same three as on macOS (`GiRemote_LoopbackLookup`: sender 707 draws, receiver 628, 5 of 93 positions differ, hits 93/93;
`TextureOnly_GiStores`, `TextureOnly_IrradianceDrawOrder`: the same checks). Images: 915 in the base manifest, **0 differing, 0 missing**, the five more are the new tests' own;
the `MaxBlockAvgDiff` listing equals the base. `ctest -L architecture` **30 of 30**; the report `ALLOWED DB-004 x 52, DB-006 x 30, DB-011 x 160, DB-017 x 78, DB-025 x 3`, `METRIC TOTAL 601/88`, `RESULT PASS`.
`nm -D --undefined-only libshader_shading.so | c++filt`: **461 symbols, identical to the base** (`tools/libshader-imports-after-phase4-linux.txt`; `diff` empty). `linux_sync.sh pull` / `verify`: 19 files, 0 differences.

**Approvals after the Phase 4 report (maintainer, 2026-11-10), recorded before they were applied.** The diff of the three test edits is
`tools/test-edit-patches/M8-phase4-test-fixes.diff` (made with `diff -u` against the files as staged at Checkpoint 4; it is the diff that was applied, and `git apply --check` is not needed:
it was applied by copying the edited files). No tolerance was changed.
1. **T-D `TextureOnly_IrradianceDrawOrder`:** the expected envdir is computed with `normalizevf()` itself (the cache's own normalization) instead of an exact division; the 1e-4 bound stays.
2. **T-C `TextureOnly_GiStores`:** the reader gets the same `CACHE_SAMPLE` flag as the writer; the 1e-5 bound is **not** added. The check is exact and order-independent: the stored records of the written cache
   and of the reloaded one (`forEachRecord`: position, normal, irradiance, coverage, envdir, both gradients, dP, every float) are sorted by their bytes and compared with `memcmp` as multisets, and the hit flags and
   the query point/normal in cache space (`Pc`, `Nc`) stay exact. The lookup value `C` is no longer compared: the reader lists a node's records in the reverse of the order they were written, so the same lookup
   sums in another order and `C` can differ by an ulp (shown with a scratch copy: 0.0946554467 against 0.0946554393 with equal flags). An exact `C` comparison is therefore impossible for a reason shown, not loosened:
   the records, which `C` is computed from, are what is compared.
3. **T-A2 `GiRemote_LoopbackLookup`:** the draw counts are reported like the positions (`REPORTED, not fixed (OQ-5)`) and the test fails only on the hit counts (T027's approved text).
4. **In-spec fix, `remoteChannel.cpp` `CHANNELTYPE_ICACHE`:** `getCache` can answer `nullptr` for a name registered as another kind: the server reports the error, answers `NET_NACK` and returns `FALSE` like the
   other refusals, and never constructs `CRemoteICacheChannel` with null. No new test (the path needs a network peer; `GiCache_KindMismatch` covers the `nullptr` answer itself).
5. **Filed, not fixed:** GitHub #158 (JIT `jitOcclusionBatch` leaves its output unwritten when `getCache` answers `nullptr`) and #159 (`getTexture3d` returns a wrong-kind frame file cast to `CPointCloud *`), both P1.
6. **`data-model.md`:** `CCacheSample` is standalone and standard-layout (the record's fields and `next`), not derived from `CIrradianceRecord`.
7. **No git in any test or build step** (maintainer's ruling): none was introduced.

**Checkpoint 4 after the approved fixes (2026-11-10).** macOS rebuilt (`032-p4b-build`, `-j4`, `-Werror`, exit 0). The ten new/characterization tests pass
(`GiFixture_ICache`, `Preview_DataSceneUnchanged`, `GiFiles_Unchanged`, `GiRemote_LoopbackCache`, `GiRemote_LoopbackPointCloud`, `GiRemote_LoopbackLookup`, `GiCache_KindMismatch`,
`TextureOnly_Standalone`, `TextureOnly_GiStores` (64 stored records and all hits equal after the reload), `TextureOnly_IrradianceDrawOrder` (7 draws, the blend equals the expected one)).
`GiRemote_LoopbackLookup` reports hits sender 93, receiver 93, draws 707 against 628, 12 positions differing (OQ-5), and passes. Full suite `-LE determinism-sweep`: **1107 of 1107 passed**.
Images: the 915 base images **0 differing, 0 missing** (5 more are the new tests' own); the `MaxBlockAvgDiff` listing equals the base. `ctest -L architecture` 30 of 30, `ALLOWED DB-004 x 52, DB-006 x 30, DB-011 x 160,
DB-017 x 78, DB-025 x 3`, `METRIC TOTAL 601/88`, `RESULT PASS`; allowlist 323.
Linux (pushed 3,778 files, 0 differences; GCC 13 `-Werror` build exit 0): **1108 tests, 1107 passed; the one failure is `TextureTile_RenderByteIdentical` (#45)**; images 0 differing, 0 missing; listing equal;
`ctest -L architecture` 30 of 30, `RESULT PASS`; `libshader_shading.so` undefined symbols identical to the base (461); pull/verify 19 files, 0 differences.
