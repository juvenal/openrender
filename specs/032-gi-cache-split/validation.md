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
- Defects found in this spec and filed by the maintainer, **none fixed here**: #158 (JIT `jitOcclusionBatch` leaves its output unwritten with no cache), #159 (`getTexture3d` returns a wrong-kind frame file cast to `CPointCloud *`),
  #160 (a photon store filled and not read from a file has a null lookup-cache root: `CPhotonStore::insert` dereferences it) and **#161** ("A client places received irradiance cache records without the server's error multiplier", P1: the cause
  was read from `irradianceDispatch.cpp:623-624` against `remoteChannel.cpp:582`, **not measured on the starting tree**; it is what T-A2 reports as OQ-5: 707 against 628 draws, 12 of 93 positions).
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
   `dP` without the sender's `1/K` (OQ-5, #161), so its tree is deeper and the lookup visits fewer nodes: a different draw count is the same difference seen through the callback. Proposed edit: report the draw counts like the
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
3. **T-A2 `GiRemote_LoopbackLookup`:** the draw counts are reported like the positions (`REPORTED, not fixed (OQ-5, #161)`) and the test fails only on the hit counts (T027's approved text).
4. **In-spec fix, `remoteChannel.cpp` `CHANNELTYPE_ICACHE`:** `getCache` can answer `nullptr` for a name registered as another kind: the server reports the error, answers `NET_NACK` and returns `FALSE` like the
   other refusals, and never constructs `CRemoteICacheChannel` with null. No new test (the path needs a network peer; `GiCache_KindMismatch` covers the `nullptr` answer itself).
5. **Filed, not fixed:** GitHub #158 (JIT `jitOcclusionBatch` leaves its output unwritten when `getCache` answers `nullptr`) and #159 (`getTexture3d` returns a wrong-kind frame file cast to `CPointCloud *`), both P1.
6. **`data-model.md`:** `CCacheSample` is standalone and standard-layout (the record's fields and `next`), not derived from `CIrradianceRecord`.
7. **No git in any test or build step** (maintainer's ruling): none was introduced.

**Checkpoint 4 after the approved fixes (2026-11-10).** macOS rebuilt (`032-p4b-build`, `-j4`, `-Werror`, exit 0). The ten new/characterization tests pass
(`GiFixture_ICache`, `Preview_DataSceneUnchanged`, `GiFiles_Unchanged`, `GiRemote_LoopbackCache`, `GiRemote_LoopbackPointCloud`, `GiRemote_LoopbackLookup`, `GiCache_KindMismatch`,
`TextureOnly_Standalone`, `TextureOnly_GiStores` (64 stored records and all hits equal after the reload), `TextureOnly_IrradianceDrawOrder` (7 draws, the blend equals the expected one)).
`GiRemote_LoopbackLookup` reports hits sender 93, receiver 93, draws 707 against 628, 12 positions differing (OQ-5, #161), and passes. Full suite `-LE determinism-sweep`: **1107 of 1107 passed**.
Images: the 915 base images **0 differing, 0 missing** (5 more are the new tests' own); the `MaxBlockAvgDiff` listing equals the base. `ctest -L architecture` 30 of 30, `ALLOWED DB-004 x 52, DB-006 x 30, DB-011 x 160,
DB-017 x 78, DB-025 x 3`, `METRIC TOTAL 601/88`, `RESULT PASS`; allowlist 323.
Linux (pushed 3,778 files, 0 differences; GCC 13 `-Werror` build exit 0): **1108 tests, 1107 passed; the one failure is `TextureTile_RenderByteIdentical` (#45)**; images 0 differing, 0 missing; listing equal;
`ctest -L architecture` 30 of 30, `RESULT PASS`; `libshader_shading.so` undefined symbols identical to the base (461); pull/verify 19 files, 0 differences.

### Phase 5 (T078 to T093): photon map split

**PAUSE 5a (T078), open.** M1 to M4 and M6b re-grepped on the Phase 4 tree (HEAD `4cba8c2c`): `test_photon_map_frame_release.cpp:44`, `test_photon_map_sessions.cpp:45`, `test_data_world_init.cpp:7`,
`test_dataview_chunking.cpp:5` (each `#include "ri/hiders/photonMap.h"`), and the end of the texture-only library definition of `tests/unit/texture_only/CMakeLists.txt` after M6a; all five patches in
`tools/test-edit-patches/` pass `git apply --check` and equal the approved text of T032. `tests/visual/test_photon_solar0.cpp` is not edited (its comment at `:60` naming `src/ri/hiders/photonMap.h` goes stale).
T078a preconditions checked by grep: `CPhotonRay` appears only in `photonMap.h:63-66` (its banner and class), and `CRay`/`ray.` appear nowhere else in `photonMap.{h,cpp}`. Nothing is edited before the maintainer answers.

**PAUSE 5a approved (maintainer, 2026-11-10).** M1 to M4 and M6b are applied at T087 exactly as shown above (a difference from the approved diff stops the work); the one-line stale comment at
`tests/visual/test_photon_solar0.cpp:60` is changed to name `src/ri/render/photonMap.h` (the only edit to that file); T078a (dead `CPhotonRay` and the `ray.h` include) may go ahead.

**T078a.** `CPhotonRay` and `#include "ray.h"` removed from `src/ri/hiders/photonMap.h` in place (nothing else used `CRay`): full build clean, `ctest -L architecture` 30 of 30 with the allowlist at 323 and the metrics unchanged,
`photonmap_*|PhotonSolar0_*` 5 of 5 (`032-p5-prebuild`).

**Test gate (T079).** Four allowlist lines deleted by key (DB-025 `photonMap.h -> dataView.h` and `photonMap.cpp -> CDataView::drawPoints`; DB-011 `photonMap.cpp -> CRenderer::toWorld`, `-> fromWorld`); headers DB-025 `(3)` -> `(1)`,
DB-011 `(160)` -> `(158)`; allowlist **323 -> 319**; `hiders-no-viewer` added to the default `GI032_BOUNDARY_CHECKS`. Red on the Phase 4 tree (`build-logs/032-p5-red-arch.log`): `Architecture_DomainBoundaries` fails with exactly the four
`VIOLATION` lines of the deleted keys; `GiSplit_Boundary` fails on `hiders-no-viewer` (2 matches: `photonMap.h` `dataView.h` and `CDataView`), every earlier sub-check passing.

**Implementation (T080 to T087).** Texturing: `hiders/photonMap.{h,cpp}` moved with `git mv` to `texture/photonStore.{h,cpp}`: `CPhotonStore : CMap<CPhoton>, CFileResource` (the constructor takes the creator's world matrices, `write(fromWorld, toWorld)`,
`store(..., key)` accepts and drops the key, `bound() const`, the direction tables and the lookup cache move with it; no `CRenderer`, `CShadingContext`, `CDataView`, `CMapRefTarget` or `IPhotonLookup` mention, no `object.h`/`raytracer.h`).
Shading: `photonMapAccess.h` (`IPhotonEstimator`, `IPhotonStore`, `CPhotonMapRefTarget`, `photonMapOf`). Core: `render/photonMap.h`, header-only `CPhotonMap : CPhotonStore, CPhotonMapRefTarget, CPhotonMapView` (one `CRefCounter` base; `write(NULL)`
writes the renderer's matrices). Consumer: `dataviewer/photonMapView.{h,cpp}` (the old draw body, moved; its member is `photons`, not `store`, because `CPhotonMap::store` must stay unambiguous). Hiders: `photon.{h,cpp}` go through
`CPhotonMapRefTarget` (`isModifying`/`setModifying`/`resetPhotons`/`storePhoton(..., 0)`/`writePhotons(CRenderer::world)`), `irradianceSampler.cpp` through `estimate`. `CRenderer::fromWorld/toWorld` are set only at `renderer.cpp:302-303` from `x->from/to`, and
the hider (`contexts[i]`, `renderer.cpp:854`) is destroyed before `world->detach()` (`:923`), so the `.pm` bytes are unchanged. `ri_sources`, the residual list cell (`rendererStatics.cpp`), the `stateHandles.h` and `photonLookup.h` comments done.
M1 to M4 and M6b applied exactly as approved (all five `git apply --check` passed first); the comment at `tests/visual/test_photon_solar0.cpp:60` now names `src/ri/render/photonMap.h` (the only change to that file).

**macOS checkpoint** (all `cpujob`): build exit 0 `-Werror` after two source slips of mine (a constructor signature, a view member named `store`). `nm -u` of `irradianceSampler.cpp.o` and `photon.cpp.o`: no `CPhotonStore`, `CPhotonMap`, `CIrradiance`, `CTexture3d`,
`CPointHierarchy`, `CDataView` or `CMap` symbol; `libshader_shading.dylib` dynamically looked-up imports **0, as the base**; `ctest -L link` 4 of 4. The photon tests: `photonmap_FrameCacheRelease`, `photonmap_TwoFrameSession`,
`photonmap_ObjectDefinitionRelease`, `PhotonSolar0_rslo`, `PhotonSolar0_slo`, `GiFiles_Unchanged` (the `.pm` of `photon-parity` at `-t:1` equal to the base in every defined field, the cache file's SHA-256 unchanged), `Preview_DataSceneUnchanged`,
`test_data_world_init`, `test_dataview_chunking`, `GiRemote_*`, `GiCache_KindMismatch` all pass. Full suite `-LE determinism-sweep`: **1107 tests, 1106 passed, 1 failed: `TextureOnly_GiStores` (SEGFAULT)**, see the finding. Images: 915 of the base **0 differing, 0 missing**
(5 more are the new tests'), the `MaxBlockAvgDiff` listing equal to the base, the photon scenes and the GI set included. `ctest -L architecture` 30 of 30; `ALLOWED DB-004 x 52, DB-006 x 30, DB-011 x 158, DB-017 x 78, DB-025 x 1` (DB-010 already gone),
`METRIC Hiders 549/78 (baseline 556/81), Texturing 0/0, TOTAL 597/86`, `RESULT PASS`; allowlist **319**; `GiSplit_Boundary` complete with all ten sub-checks.
**Linux** (pushed 3,782 files, GCC 13 `-Werror`, `-j2`): build exit 0, 1108 tests, 1106 passed, the failures being `TextureTile_RenderByteIdentical` (#45) and `TextureOnly_GiStores` (the same segfault); images 0 differing, 0 missing; listing equal; architecture 30 of 30,
the same report; `libshader_shading.so` imports identical to the base (`tools/libshader-imports-after-phase5-linux.txt`).

**Finding (Phase 5): `TextureOnly_GiStores`, photon section, cannot pass as written.** Marker prints in a scratch copy (scratchpad `032-dbg-photon.cpp`) put the crash at the fifth lookup of the freshly filled store, the first one that finds two or more photons.
`CPhotonStore::root` (the probe cache's octree) is created only when a map is read from a file (`photonMap.cpp:68`, `:107` on the starting tree; the constructor leaves it `NULL` for a new map), `probe()` tests it, but `insert()` does not: a lookup that finds photons in a
map that was never read dereferences a null root. The renderer never does this (a map is written by the photon hider and read in a later frame), which is why the base never showed it; the approved test queries the store before the reload. It need not compare equal even with a guard:
the reloaded store has a root and answers from its probe samples, the original answers uncached. This is a pre-existing defect on a path the product does not reach; the code is as on the starting tree. Proposed edit of the test (needs approval): compare the stored photons exactly after the reload
(`numItems`, and every field of every item from 1 on: `P`, `N`, `flags`, `C`, `theta`, `phi`, as the T-F reader does, with the file's maxPower and matrices) and query only stores that were read from the file (two stores read from the same file answer equally).

**Approvals after the Phase 5 report (maintainer, 2026-11-10), recorded before they were applied.**
1. The null lookup-cache root of a photon store that was filled and not read from a file is **GitHub #160** (P1, Bug Tracker), filed and **not fixed** in this spec (`CPhotonStore::insert` dereferences `root`, which only the read constructor creates). No `Closes` line.
2. **`TextureOnly_GiStores`, photon section, edited as proposed** (`tools/test-edit-patches/M9-phase5-photon-section.diff`, made with `diff -u` against the file as staged at Checkpoint 5; it is the diff applied). No tolerance anywhere: after `write(fromWorld, toWorld)` with exactly
   representable non-identity matrices (scale 2, translate (1, 2, 4), and the inverse) the written store's photons are captured; two stores are then read from the file with identity world matrices and compared exactly: `numItems`; every field of every item from 1 on (`P`, `N`, `C`, `flags`, `theta`, `phi`);
   `maxPower` equal to what `write()` put into the file; and the matrices (against identity the reader's `to` is the file's `fromWorld` and its `from` the file's `toWorld`). The estimates are queried only on the two stores read from the file and compared with `memcmp`; the store that was filled and not read is never queried.

**Checkpoint 5 after the approved edit (2026-11-10).** macOS rebuilt (`032-p5b-build`, `-j4`, `-Werror`, exit 0). `TextureOnly_GiStores` passes (64 stored cache records and the hits after the reload equal; **126 stored photons**, `maxPower` and matrices after the
reload equal those written, and two readers answer alike). Full suite `-LE determinism-sweep`: **1107 of 1107 passed**. Images: the 915 base images **0 differing, 0 missing** (5 more are the new tests'); the `MaxBlockAvgDiff` listing equals the base; the photon scenes and the GI
set are byte-identical. `ctest -L architecture` 30 of 30; `ALLOWED DB-004 x 52, DB-006 x 30, DB-011 x 158, DB-017 x 78, DB-025 x 1`; `METRIC Hiders 549/78 (baseline 556/81), Texturing 0/0, TOTAL 597/86`; `RESULT PASS`; allowlist **323 -> 319**.
Linux (pushed 3,784 files; GCC 13 `-Werror`, `-j2`; build exit 0): **1108 tests, 1107 passed, the one failure is `TextureTile_RenderByteIdentical` (#45)**; `TextureOnly_GiStores` passes; images 0 differing, 0 missing, listing equal; `ctest -L architecture` 30 of 30, same report;
`libshader_shading.so` imports identical to the base (461). `.pm` of `photon-parity` at `-t:1` equal to the base in every defined field (`GiFiles_Unchanged`, both platforms). Checkpoint 5 (T089 to T093) is ticked.

### Phase 6 (T094 to T101): boundary close-out

**T094.** Stale comments fixed (comment-only; no test comment is edited): `render/rendererFileSearch.cpp` (the `irradianceDispatch.cpp` pair), `hiders/depository.cpp` (`depository.h` is no longer included by the cache's header: nothing includes it now; the file stays only until
the SSS bake is finished or retired), `dataviewer/dataLoad.cpp` and `dataviewer/dataView.cpp` (the moved files' names), `preview/libribpreview/dataScene.h`, `libshader/shading/shading.h` (`CIrradianceSampler::sample`), `libshader/shading/photonLookup.h` (`CPhotonStore::lookup`).
**T096 audit.** `grep -c '^DB-'` = **319** (DB-004 52, DB-006 30, DB-011 158, DB-017 78, DB-025 1; DB-010 and DB-020 none); the headers read `(158)`, `(1)` and the DB-010 header is gone; the `surface.cpp` DB-025 line is present, so DB-025 stays Open.
`git diff e83d005d` over the allowlist shows deletions and the two rewritten section headers only; over `ribvector-core-residual.txt` the stub row deleted and the `rendererStatics.cpp` cell rewritten (as T087 left it); nothing added.
**T097, quickstart section 1, literally:** the `src/ri/texture` scan prints nothing; `grep -rn 'friend ' src/ri/texture src/ri/hiders` prints only `friend void makeBrickMap` (`pointCloud.h:144`); no `src/ri/texture` header is included under `src/ri/hiders`; the five `files-absent` files are absent and
`irradianceSampleAccept` appears nowhere under `src/`. All ten `GI032_BOUNDARY_CHECKS` sub-checks are on and `GiSplit_Boundary` is green.
**T098 (macOS).** Rebuilt (`032-p6-build`, `-j4`, exit 0). `ctest -R 'GiRemote_|GiSplit_|GiCache_|SceneInterface_|Preview_NoCoreReach'` **17 of 17**; `ctest -L architecture` **30 of 30**; the report `ALLOWED DB-004 x 52, DB-006 x 30, DB-011 x 158, DB-017 x 78, DB-025 x 1`, `RESULT PASS`;
the metrics still show the four `NOTE metric ... < baseline` lines (Hiders 549 < 556 and 78 < 81, TOTAL 597 < 604 and 86 < 89) until the approved baseline is applied. **T-A2 on the finished tree:** hits sender 93, receiver 93; draws sender 707, receiver 628; 12 of 93 positions differ (OQ-5, #161, reported, not fixed).
**T095, T099 (for approval, PAUSE 6a).** `tools/test-edit-patches/B1-metrics-baseline.diff` (the baseline file: Hiders 556/81 -> 549/78, TOTAL 604/89 -> 597/86, one comment paragraph; Texturing 0/0) and `registry-draft.md` are shown to the maintainer; neither is applied yet.

**PAUSE 6a approved (maintainer, 2026-11-10); applied.** `tools/test-edit-patches/B1-metrics-baseline.diff` applied exactly as shown (`metrics-baseline.txt`: Hiders 549/78, TOTAL 597/86, the spec 032 comment; Texturing 0/0).
`registry-draft.md` applied to `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` with the date 2026-11-10: DB-010 and DB-020 moved to Resolved (spec 032, with the stale text corrected: `texture3d.cpp` has not included `renderer.h`/`displayChannel.h` since spec 037;
spec 030 has no friend left to remove); history entries for DB-025 (12 -> 1, stays Open for `surface.cpp`), DB-017 (the irradiance pair deleted, 78 lines, stays Open), DB-011 (6 lines deleted, 164 -> 158, metric 556 -> 549) and DB-024 (the viewer's
Hiders reach gone; `CONSUMER` total 71); DB-023 unchanged. `CLAUDE.md` and `DEVNOTES.md` are not edited.
**After the edits:** `ctest -L architecture` 30 of 30 on macOS and on Linux; the report `ALLOWED DB-004 x 52, DB-006 x 30, DB-011 x 158, DB-017 x 78, DB-025 x 1`, `METRIC Hiders 549/78 (baseline 549/78), Texturing 0/0, TOTAL 597/86 (baseline 597/86)`,
**no `NOTE` line**, `RESULT PASS`. Linux Phase 6 (remote rebuild with the ten-check list, GCC 13 `-Werror`, exit 0, no error): `GiRemote_|GiSplit_|GiCache_|SceneInterface_|Preview_NoCoreReach` **17 of 17**; pull/verify 3,786 files, 0 differences. Checkpoint 6 (T098, T100, T101) ticked.

**For the merge (maintainer's note).** Spec 031 also edits the Hiders and TOTAL rows of `metrics-baseline.txt`, the two DB-011 `photonMap.cpp` lines (`photonMap.cpp -> CRenderer::toWorld` and `::fromWorld`), and `hiders/photonMap.cpp` itself (which this spec moved to
`texture/photonStore.cpp`). Whichever spec merges second keeps 032's photonMap layout (the store in Texturing, `render/photonMap.h` in Core) and re-measures the metric rows on the merged tree.

**T-A2's difference and the starting tree (from the records only, no new run).** The records do not show it on the starting tree. T-A2 was written as a finished-tree test ("Finished tree only", T027) and ran for the first time in Phase 4, so no lookup, draw-count or position
comparison between sender and receiver exists for the base (Phase 0/1 recorded only the loopback bytes: `GiRemote_LoopbackCache`, whose receiver wrote 118 nodes for F1's 93 records, and `GiRemote_LoopbackLookup` was not there). The records do support that the cause is the base's:
the receiver's written cache file equals the base recording byte for byte on the finished tree (`GiRemote_LoopbackCache` passes against the reference taken on the starting tree), so the receiver's tree (placement by `dP` without the sender's `1/K`, `remoteChannel.cpp:582` on the base) is the base's,
and `lookupCached` is the base's `lookup` arithmetic (`TextureOnly_IrradianceDrawOrder`). A different tree gives different visited-node sets, hence the 707 against 628 draws and the 12 differing positions; but that is an inference, not a base measurement.

### Phase 7 (T102 to T113): keys for spec 040

**PAUSE 7a (T102), open.** Phase 7 edits one approved test: `tests/gi_split/test_gi_remote_loopback.cpp`, `maskCacheFrame` (T-A's mask of the cache frame). Proposed diff: `tools/test-edit-patches/M10-phase7-loopback-key-strip.diff` (not applied; made with `diff -u` against the file as committed).
Besides masking `next` it strips the 8 key bytes at offset 224 of each 240-byte `CCacheSample` (a `static_assert` pins the layout: 224 bytes of fields, the key, `next` at 232) and compares the 232-byte records with the recording taken on the starting tree; the reference is not re-recorded.
T-E (`GiKeys_*`) stays inside the `OPENRENDER_032_RED_TESTS` block until T108a. No source file is edited before the maintainer answers. #161 (T-A2's OQ-5 receiver placement) is recorded in §6 with #158 to #160.

**PAUSE 7a approved (maintainer, 2026-11-11).** `tools/test-edit-patches/M10-phase7-loopback-key-strip.diff` is approved exactly as written and applied at T103 (a difference from it stops the work). The reference recordings are not re-recorded.

**T104: the draw cursor between `bindDraws()` and the sampler's work (read from `shading.h`/`shading.cpp`, no run).** `drawCursor` (a `SubstreamKey`: `seedHash`, `indexBase`) is *written* in exactly three places: `bindDraws()` (`shading.cpp:812-817`: `drawCursor = drawSubstream(vertex, counts,
callSite, samplesPerCall)` and `drawCursorDim = 0`, a no-op only when `counts` is null, which cannot happen at the two call sites because `DRAW_BEGIN`/`jitOcclusionBatch` take `drawCounters(callSite)`, which allocates its occurrence table on demand),
`fillPointKeys()` (`:848`, reset to `{0, 0}` when a new shading invocation begins) and `resetDrawOccurrences()` (`:808`, the photon hider's batch start). Everything else only reads it or moves `drawCursorDim`: `keyedNext()` (`shading.h:1051`, `drawCursorDim++`),
`drawNext()`/`drawNext2()`/`drawNext4()`, `commitTextureDraws()` (`:579`, `drawCursorDim = opt.stream.dim`), and `drawBound2D()`, which reads the cursor by value. So, between the `bindDraws()` of the call and the end of `irradianceLookup`:
(1) the store lookup visits records and its draw callback is `drawNext()`, which advances `drawCursorDim` only, so **the key is independent of cache occupancy and of how many records the lookup visited**; (2) the sampler's `drawNext2/drawBound2D/lookupEnvironment` (`commitTextureDraws`) touch only the dimension;
(3) its probe rays (`libshader::traceRay`, occlusion/indirectdiffuse probes) do not shade, and a nested shading on the same context would reach `bindDraws`/`fillPointKeys` and overwrite the cursor, which is why `irradianceLookup` copies `ctx.boundDraws()` **at its top**, before the store lookup and the sampler: the copy cannot see any of it.
The copy is thus a pure function of the call's sub-stream (the point's key, the shader, the call site, the occurrence and the ray depth, `drawSubstream()`), the same for the interpreter's `IDEXPR` and the JIT's `jitOcclusionBatch` (both call `bindDraws` first).

**Implementation (T105 to T108a).** `core/map.h`: the protected virtual `permuted(sorted, oldItems, count)` (empty), called by `balance()` after `finalitems` is filled and before the old array is freed; nothing else of `balance()` changed. `texture/photonStore.{h,cpp}`: the side array `keys` (index-aligned,
`ensureKeys()` grows it to `maxItems + 1` under the store's existing lock, zero-filled), `store(P, N, I, C)` stores key 0 and calls the keyed one, `reset()` frees, the read constructor allocates it zeroed, the dummy photon of an empty map has key 0, `permuted()` builds `newKeys[i] = keys[sorted[i] - oldItems]`,
`photonKey(i)`; `CPhoton` stays 44 bytes and `write` is the raw `fwrite` as before. `libshader/shading/photonMapAccess.h`: `photonKey(pointKeyFull, bounce) = mix(0xF07011BE, ...)`; `hiders/photon.{h,cpp}`: `tracePhoton` takes the point key (`pointKeys[numVertices - i].full`, the index `bindDraws` uses) and the two `storePhoton`
calls pass `photonKey(pointKeyFull, numDiffuseBounces + numSpecularBounces)`. Irradiance record: `CIrradianceRecord::key` (offset 224) and `CCacheSample::key` (so `next` is at 232 and **`sizeof(CCacheSample)` goes 232 -> 240**; the `static_assert(offsetof(next) == sizeof(CIrradianceRecord))` still holds), `get`/`set` copy it,
`insertReceived` copies the whole sample, `readNode` sets `key = 0` and `writeNode` is unchanged (no file carries a key); `shading.h`: `boundDraws()`; `irradianceCalls.{h,cpp}`: `irradianceRecordKey(k) = mix(0x1CAC4E5A, k.seedHash, k.indexBase)`, computed from the copy taken at the top of `irradianceLookup`. M10 applied (T103); M7 applied (`CContractForwarder` no longer `final`,
approved 2026-11-09). T-E moved out of the red block (T108a; the block is gone). Two build-only deviations in the approved `tests/gi_split/CMakeLists.txt` definitions, found when T-E first built: `test_gi_keys` needs `libshader_shading` on its link line (it names `CShadingContext::s_defaultServices`), and its `argc != 4` test needs a third argument in the
parent cases, so they pass the placeholder `-` (as `GiCache_KindMismatch` does); the test source is not edited.

**macOS checkpoint (T109 to T111).** Build `-j4` exit 0 under `-Werror` (two build-only fixes to the approved CMake definitions of T-E, listed above). `GiKeys_Photons` passes: 953 photons in each of two `-t:1` runs, **953 distinct keys**, and the two sets equal; `GiKeys_IrradianceRecords` passes: 37 records
in each run, 37 distinct keys, sets equal (so the records two runs share have equal keys, OQ-3). `GiRemote_LoopbackCache` passes against the unchanged base recording **with the 8 key bytes stripped and nothing else different** (93 records, `CCacheSample` is 240 bytes: **wire size 232 -> 240 bytes per record**: F1's 93 records are 21,576 -> 22,320 bytes, 8 more per record, and the frame 21,752 -> 22,496 bytes with the node counts and the terminator unchanged);
`GiRemote_LoopbackPointCloud`, `GiRemote_LoopbackLookup` (T-A2: still 707 against 628 draws, 12 positions, #161), `GiCache_KindMismatch`, `GiFiles_Unchanged` (no file carries a key), `TextureOnly_*` and `PhotonSolar0_*` (44 bytes) pass, and so do the three #64 tests. Full suite
`-LE 'determinism-sweep|gi_split_threads'`: **1109 of 1109 passed**. Images: the 915 base images **0 differing, 0 missing** (6 more are the new tests' own, one of them T-E's cache render); the `MaxBlockAvgDiff` listing equals the base: with keys, no GI image changed (FR-018). `ctest -L architecture` 30 of 30, `RESULT PASS`, allowlist 319, metrics 549/78 and 597/86 as the baseline;
`ctest -L link` 4 of 4; `libshader_shading.dylib` dynamically looked-up imports **0, as the base**.

**T110: `GiKeys_PhotonsThreads` (`-t:1` against `-t:4`, opt-in, run three times alone through `ctest -L gi_split_threads`).** Run 1: the `-t:4` child render failed (the intermittent `CPhotonHider` crash recorded in `BUGS.md`, "Intermittent SIGSEGV in `CPhotonHider` photon-map construction"; re-run alone, not fixed). Runs 2 and 3: both children finish with 953 photons and the test **fails on the comparison**:
the sets differ in 268 entries. I compared the two dumps (scratchpad `032-ka.txt`, `032-kb.txt`): the **keys are identical, all 953 of them**, and with the `flags` column removed the (key, `P`, `N`, `C`, `theta`, `phi`) sets are identical too (0 differing lines). `flags` is the kd-tree's splitting axis that `balance()` assigns to each item (0, 1 or 2: 635/157/161 against 632/157/164 photons),
a function of the order the photons were stored in, which differs between one thread and four. So the key property of US5 (a photon's key is the same at every thread count) holds; the test's property also includes `flags`, which is a layout fact and not part of a photon's identity. This needs an edit of the approved test (**proposed, not made**): compare the
photon entries without `flags` in the `-t:1` against `-t:4` case (`GiKeys_Photons`, `-t:1` twice, keeps it: the insertion order is the same there). Until it is approved `GiKeys_PhotonsThreads` stays red (it is opt-in, outside the everyday run; the suites above exclude it by its label).

**Linux checkpoint (T112; GCC 13 `-Werror`, `-j2`, `openrender-032`; the remote lock was held by another spec's sweep for about 40 minutes first).** Build exit 0, no error and no warning. The suite `-LE determinism-sweep` (which, on Linux, still includes the opt-in test): **1111 tests, 1109 passed**; the failures are `TextureTile_RenderByteIdentical` (the baseline failure #45) and
`GiKeys_PhotonsThreads`. `GiKeys_Photons` and `GiKeys_IrradianceRecords` pass. `GiRemote_LoopbackCache` passes against the Linux base recording with the key stripped. Images: the 915 base images 0 differing, 0 missing (6 more are the new tests' own); the `MaxBlockAvgDiff` listing equals the base. `ctest -L architecture` 30 of 30, same report, `RESULT PASS`;
`libshader_shading.so` undefined symbols identical to the base (`tools/libshader-imports-after-phase7-linux.txt`; pull/verify 21 files, 0 differences). `ctest -L gi_split_threads` once: both children finish with 953 photons, and the comparison fails the same way as on macOS: the dumps' keys are identical (0 differing lines over 953) and with `flags` removed the entries are identical (0 differing lines).

**Approvals after the Phase 7 report (maintainer, 2026-11-11), recorded before they were applied.**
1. **M11** (`tools/test-edit-patches/M11-phase7-photons-threads-flags.diff`): in `compare()` of `tests/gi_split/test_gi_keys.cpp` the 8th field of a photon line (`flags`, the kd-tree splitting axis) is dropped from both dumps, only when the two thread counts differ and only for the photon dump; the file's header comment says so. `GiKeys_Photons` (`-t:1` twice) and the record cases still compare every field. No tolerance anywhere.
   The spec text that listed `flags` among the compared fields is corrected to match (`contracts/gi-keys.md` property 1, `tasks.md` T030).
2. **Accepted after the fact:** the two build-only changes to the approved T-E definitions in `tests/gi_split/CMakeLists.txt`: `libshader_shading` on `test_gi_keys`'s link line, and the `-` placeholder third argument in the three parent cases (`GiKeys_Photons`, `GiKeys_IrradianceRecords`, `GiKeys_PhotonsThreads`).
3. **The intermittent `CPhotonHider` crash** (`DEVNOTES_DETAILS/BUGS.md`, "Intermittent SIGSEGV in `CPhotonHider` photon-map construction") is **GitHub #164** (P1), filed by the maintainer. A GiKeys case that hits it is recorded here as that issue and re-run; nothing is widened.

**M11 applied; Checkpoint 7 closed (2026-11-11).** macOS (`032-p7b-keys`, `-j4`, build clean): `GiKeys_Photons`, `GiKeys_IrradianceRecords` and `GiKeys_PhotonsThreads` pass in all three rounds, and `ctest -L gi_split_threads` alone passes in all three (the `-t:4` children finish with 953 photons; the keys, and the entries without `flags`, are equal to `-t:1`'s).
No `CPhotonHider` crash on macOS in these six runs. `ctest -L architecture` 30 of 30, `RESULT PASS`, metrics as baselined (549/78, 597/86), allowlist 319. The rest of the macOS checkpoint is the full run recorded above: nothing in `src/` changed since (only `test_gi_keys.cpp`, its comment and the spec text).
Linux (remote `cpujob` `-j2`, GCC 13 `-Werror`, the rebuild of `test_gi_keys` clean): the three cases pass in three rounds and `ctest -L gi_split_threads` alone passes, **except one round in which `GiKeys_PhotonsThreads` failed with "a child render failed" after 2.26 s: the intermittent `CPhotonHider` crash, which is GitHub #164 (P1; BUGS.md); the case was
re-run alone straight away and passed, as recorded, nothing widened.** `ctest -L architecture` 30 of 30, `RESULT PASS`. T113 is ticked.

### Phase 8 (T114 to T124): guardrails, documents, other configurations, timing, full runs and sweeps

**T114.** `kRendererServicesVersion` 2.1.0 -> **2.2.0** with the contract's history entry (`photonMapAccess.h`, `irradianceSampler.h`, `irradianceCalls.h`, `boundDraws()`; `getCache()` wording and Core's kind check): additions only, no member changed, the implementers' `static_assert(MAJOR == 2)` unchanged,
`photon_services.h:9` (a test comment) not edited. Rebuilt (`032-p8-build`, `-Werror`, exit 0); `-L 'shading_parity|shading_contract'` **205 of 205** (`032-p8-labels`).
**T115.** The `CLAUDE.md`/`DEVNOTES.md` edits are drafted in `claude-md-draft.md` (ten edits, each old -> new): gotchas 24, 25 and 28, a new gotcha 36, the "Graphics State handles" paragraph, a new "GI stores" paragraph, "Active Technologies", "Recent Changes", the Testing block and the `DEVNOTES.md` status row.
**T116 (PAUSE 8a) is open: nothing in `CLAUDE.md` or `DEVNOTES.md` is edited.**

**T117, the other configurations (macOS, `-j4`, `-Werror`; trees kept, not deleted: the maintainer's rule is to ask first).** `build-nojit` (`-DOPENRENDER_ENABLE_JIT=OFF`, the interpreter alone): configure + full build exit 0, `ctest -L plugin_abi` **64 of 64** (the JIT build has 71). `build-tsan` (`-DOPENRENDER_TSAN=ON`, builds only, Q8): configure + full build exit 0, no test run (#122).
No-OpenEXR: the tree supports it through CMake's standard `-DCMAKE_DISABLE_FIND_PACKAGE_OpenEXR=ON` (the root `CMakeLists.txt` finds OpenEXR with `find_package(OpenEXR QUIET CONFIG)` and has no option of its own): `build-noexr` configures ("OpenEXR not found -- EXR display driver will not be built") and builds, exit 0, no error. The three trees hold 456, 457 and 334 MB.
**T118 timing (SC-007), `timing_sc007.sh build-base build 5`, `-t:4`, medians (spread) start -> final:** macOS perf-occlusion-cache 5.72 (0.87) -> 5.57 (0.99), perf-indirect-photon 6.39 (1.28) -> 6.44 (0.41), perf-ptc 3.99 (0.83) -> 3.87 (0.60); **Linux** (`perf-ptc` at `-t:1`, #122) 9.19 (1.05) -> 9.04 (0.23), 13.66 (0.04) -> 13.95 (0.36), 6.86 (0.14) -> 6.91 (0.34).
Differences of medians: -2.5%, +0.8%, -2.9% (macOS), -1.5%, +2.1%, +0.6% (Linux); every one **within the larger spread; scenes slower beyond the spread 0; scene medians under 3 s: 0** on both platforms (`build-logs/032-p8-timing.log`, `build-logs/linux/032-l-timing.log`).

**T119 full macOS run.** Build exit 0. `ctest -LE determinism-sweep -j4`: **1110 of 1110 passed** (every label of SC-009 is in it; the counts per label: architecture 30, visual 532, shading_parity 136, texture_tile 15, texture_only 3, state 7, plugin_abi 71, link 4, preview 22, gi_split 8, common 18, libshader 28). CP-d: the 915 base images **0 differing, 0 missing** (6 more are the new tests' own), the `MaxBlockAvgDiff`
listing equals the base; CP-g (the GI set) is inside it. `ctest -L architecture` 30 of 30, `RESULT PASS`, `ALLOWED DB-004 x 52, DB-006 x 30, DB-011 x 158, DB-017 x 78, DB-025 x 1`, metrics 549/78 and 597/86 against a baseline of the same values, no `NOTE`; `ctest -L link` 4 of 4; `libshader_shading.dylib` dynamically looked-up imports 0 (as the base). A standalone re-run of `GiKeys_PhotonsThreads` straight after the suite
(`032-p8-rerun`) **failed with "a child render failed" after 0.05 s: the intermittent `CPhotonHider` crash, GitHub #164**, recorded as that issue; the case passed inside the suite and passed on the Linux re-run; its second standalone re-run was queued behind other work and is recorded below if it ran.
**T120 full Linux run** (remote `cpujob` `-j2`, GCC 13 `-Werror`, pushed 3,790 files): build exit 0, no warning; the everyday run **1111 tests, 1110 passed**, the one failure is `TextureTile_RenderByteIdentical` (#45); CP-d against the `-linux` baseline: 915 images **0 differing, 0 missing** (6 more are new tests'), listing equal; `ctest -L architecture` 30 of 30, `RESULT PASS`, same report;
`libshader_shading.so` undefined symbols identical to the base (461; `libshader-imports-after-phase8-linux.txt`); `GiKeys_PhotonsThreads` run alone: passed. Pull/verify: 22 files, 0 differences.
**T121, macOS determinism sweep** (`-L '^determinism-sweep$' -j2`, stamped first): **419 of 419 passed, 0 differing, 0 missing** against `baseline-sweep-images-macos.sha256`; no `bake3d` crash. **T122, Linux sweep** (`-j2`, remote `cpujob`): **419 of 419 passed, 0 differing, 0 missing** against `baseline-sweep-images-linux.sha256`; no `bake3d` crash.

### §7 final walk, SC-001 to SC-010 (T123)

| SC | Evidence (command, result, file) |
|---|---|
| **SC-001** | `grep -rnE 'CShadingContext\|CRenderer\|CDataView\|CRemote\|CMapRefTarget\|IPhotonLookup\|dataView\.h\|debug\.h' src/ri/texture` prints nothing (T097, re-checked at Phase 8 on the committed tree); `GiSplit_Boundary` green with all ten sub-checks; `archcheck` reports Texturing 0/0 and no Texturing-origin allowlist line (`build-logs/032-p8-arch.log`, `build-logs/linux/032-l-p8-arch.log`). |
| **SC-002** | `grep -c '^DB-' tests/architecture/allowlist.txt` = **319** (344 at the start; DB-004 52, DB-006 30, DB-011 158, DB-017 78, DB-025 1; DB-010 and DB-020 gone), headers match, `git diff e83d005d` over the allowlist shows deletions and two rewritten headers only (T096). |
| **SC-003** | The GI set (`tools/gi-test-set.txt`, inside `ctest -LE determinism-sweep`) passes and its images are byte-identical on both platforms: 915 base images 0 differing, 0 missing, every `MaxBlockAvgDiff` equal, at the commits of Phases 3 to 8 (`build-logs/p8-images.sha256`, `p8-diffs.txt`, `build-logs/linux/p8-*`); both sweeps 419 of 419 and 0 differing. |
| **SC-004** | `Preview_DataSceneUnchanged`, `test_data_*`, `test_dataview_chunking`, `test_debugdump_roundtrip`, `test_wire_cli` pass unchanged on both platforms (the `-L preview` set: 22 tests, in the 1110/1111); the recorded `DataScene` of the five document types equals the base recording. |
| **SC-005** | `GiRemote_LoopbackCache` and `GiRemote_LoopbackPointCloud` pass against the recordings taken twice on the unmodified tree (the cache frame with the 8-byte key stripped since Phase 7, M10; the receiver's written file byte for byte; the point-cloud setup tail and padding masked). `GiRemote_LoopbackLookup` (T-A2) reports hits 93/93, draws 707 against 628 and 12 of 93 positions differing: OQ-5, **#161, reported, not fixed**. |
| **SC-006** | `TextureOnly_GiStores` (cache section: the stored records equal exactly as a multiset after the reload; photon section: the stored photons, `maxPower` and matrices equal exactly, two readers answer alike) and `TextureOnly_IrradianceDrawOrder` pass on both platforms (`-L texture_only`, 3 tests); the program links Infra and the texture library only (`texturing_only_gi_sources`). |
| **SC-007** | T118 above: every scene's median within the larger spread on both platforms, none slower beyond the spread, none under 3 s (`build-logs/032-p8-timing.log`, `build-logs/linux/032-l-timing.log`; scenes `tools/perf-*.rib`). |
| **SC-008** | `grep -rn 'friend ' src/ri/texture src/ri/hiders` shows `makeBrickMap` only; DB-020 Resolved (T100); `irradianceDispatch.cpp`, `irradianceDispatchStub.cpp`, `radiance.{h,cpp}`, `harmonics.h` and `irradianceSampleAccept` are gone (`files-absent`); `ctest -L link` 4 of 4; `libshader_shading` imports: 0 dynamically looked-up (macOS, as the base) and the same 461 undefined symbols (Linux); `ribvector-core-residual.txt` lost the stub row only. |
| **SC-009** | The label set (architecture 30, visual 532, shading_parity 136, texture_tile 15, texture_only 3, state 7, plugin_abi 71, link 4, preview 22, gi_split 8, common 18, libshader 28) is inside the everyday runs: macOS **1110 of 1110**, Linux **1110 of 1111** (the failure is #45, the baseline failure, `TextureTile_RenderByteIdentical`); `GiKeys_PhotonsThreads` re-run alone when it failed (#164). |
| **SC-010** | `grep -n "NEEDS CLARIFICATION" specs/032-gi-cache-split/spec.md` prints nothing. Reading `tasks.md`, `claude-md-draft.md`, `registry-draft.md` and this file: no task, requirement or registry text depends on a merge; the only mentions of the merge are the rule itself (`tasks.md`: "Nothing depends on the merge") and the maintainer's note recorded in §6 for whichever of 031 and 032 merges second (documentation, not a task). |

**Every departure and finding for the maintainer (§6 and the phases above).** D1 to D19 of `tasks.md` (as approved); `transformBound` copied file-locally (State is not Texturing's); `CCacheSample` standalone and standard-layout; the `GiKeys` CMake fixes (accepted 2026-11-11). Filed by the maintainer, none fixed here: **#158** (JIT `jitOcclusionBatch` leaves its output unwritten with no cache), **#159** (`getTexture3d` returns a wrong-kind frame file cast to
`CPointCloud *`), **#160** (a photon store filled and not read has a null lookup-cache root), **#161** (the receiver places received records without the server's error multiplier: T-A2), **#164** (the intermittent `CPhotonHider` crash). `#122` (`bake3d` crash) did not occur in any run of this phase; `perf-ptc` stays pinned to one thread on Linux. Unchanged and untouched: #104 (name-keyed `frameFiles`: one name used for two GI kinds still answers the wrong class
from `getTexture3d`/`getPhotonMap`), #131 (`numItems` on the point-cloud wire), #145/#146/#97 (their code untouched: Shading's `photonmap()` builtins and `IPhotonLookup` are as they were). **#15**: the task (T123) says it looks stale; I did not open it and make no claim about it, for the maintainer to judge.

**PAUSE 8a approved (maintainer, 2026-11-11); T116 finished as follows.** `claude-md-draft.md` is approved with one change: spec 056 merged first (`11fa3424`) and its `CLAUDE.md` now holds gotcha 36, a "Volume shader segments (spec 056)" architecture subsection, a `-L volume` Testing line and its own Active Technologies and Recent Changes entries. The draft is updated: this spec's gotcha is **37** and every reference
to it says "gotcha 37"; the draft states that it applies after 056's text (the Active Technologies bullets after 056's, the Recent Changes entry above 056's, newest first, the new architecture paragraph next to 056's subsection, the Testing lines next to `-L volume`). **`CLAUDE.md` and `DEVNOTES.md` are applied at merge preparation**, on top of 056's text, per the maintainer: this worktree has not merged master
and 056's text is not in it, so nothing in `CLAUDE.md` or `DEVNOTES.md` is edited here (`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` was applied at T100). Master is not merged now; that happens at merge preparation.
**Build trees.** The maintainer approved deleting `build-nojit`, `build-tsan` and `build-noexr`; exactly those three were deleted (`build`, `build-base` and `build-logs` stay).
**For the merge (maintainer's note).** Spec 031 also steps `kRendererServicesVersion` from 2.1.0 to 2.2.0. Whichever of 031 and 032 merges second steps it to **2.3.0** and puts its history entry after the first one's (the contract's collision rule, OQ-7).
**#164 on macOS, T110's re-runs.** The second queued standalone re-run (`032-p8-rerun2`, three runs of `ctest -R GiKeys_PhotonsThreads`): the first failed with "a child render failed" after 0.17 s (**#164** again), the second and third passed. With the failure seen straight after the suite (`032-p8-rerun`), that is two `CPhotonHider` crashes in four standalone macOS runs of this case in Phase 8, none in the suites themselves (the case passed inside the everyday run); Linux: none. Nothing widened.

### Phase 9: convergence (T125 to T127; the converge stage, 2026-11-11)

**T125, comment and text corrections (no behaviour, no layout, no file, no allowlist change).**
(a) `src/ri/texture/photonStore.h:96`: "the key is not kept yet" now says the key goes to the side array, is read back with `photonKey(i)` and is permuted with the photons by `balance()`. A re-grep of `src/` for `not kept yet`, `constant 0` and `until Phase` finds nothing else in a file this spec created or edited (`textureInternal.h:38` is spec 027's, `RSLShadingState.h` is not this spec's).
(b) `rendererServices.h` history entry 2.2.0 no longer says "Additions only": it says **breaking within 2.x** (allowed before spec 055: a MINOR step may break while MAJOR is 2), because `getCache()` answers `nullptr`, where it answered the other object, for a name registered as a point cloud or brick map; no member or signature changed; the additions follow. The contract's copy
(`contracts/renderer-services-version.md`), `plan.md` (two places) and the CLAUDE.md draft (gotcha 28's sentence) are corrected the same way. The T114 sentence above ("additions only, no member changed") is superseded by this: the version step was always breaking within 2.x for that one behaviour, as the contract's "What does change in behaviour" says.
(c) The spec text no longer says `test_photon_solar0.cpp` is untouched: it received one comment-only line, approved at PAUSE 5a (`plan.md`, `research.md`, `spec.md`, `tasks.md`, `contracts/tests.md`).
(d) **Prepared, not applied (needs approval):** `tests/link/check_scope.cmake` S1b still names the removed `irradiance.h`: `tools/test-edit-patches/L1-link-check-scope-s1b.diff` changes the check to `irradianceCache.h` (the concrete cache) and notes that `photonMap.h` is Core's since this spec; the comment of the check follows. No file under `src/libshader` includes either header (checked by grep), so the changed check passes.
**T126.** Build (`032-p9-build`, `-j4`, `-Werror`): exit 0, no error. `ctest -L architecture` **30 of 30** (`GiSplit_Boundary` included), `RESULT PASS`, `ALLOWED DB-004 x 52, DB-006 x 30, DB-011 x 158, DB-017 x 78, DB-025 x 1`, `METRIC Hiders 549/78, Texturing 0/0, TOTAL 597/86`, equal to the Phase 8 report. Nothing else was re-run (comment and text edits).
Out of this spec's scope and left to the coordinator's GitHub items: `map.h`'s `delete`/`delete[]` mismatch and the `reset()`/`store()` race.

**(d) approved and applied (maintainer, 2026-11-11).** `tools/test-edit-patches/L1-link-check-scope-s1b.diff` applied exactly as shown to `tests/link/check_scope.cmake` (`git apply --check` passed first): S1b now names `irradianceCache.h` (the concrete cache) in place of the removed `irradiance.h`, and its comment says `photonMap.h` is Core's since this spec.
Result (`032-p9b-link`, through `cpujob`): `ctest -L link` **4 of 4** (`Link_ShadingSelfContained`, `Link_NoWorkaroundText`, `ShadingScope_Greps` with the changed S1b, `Link_StaticConsumer`); `ctest -L architecture` **30 of 30**. The repo-wide result for Phase 9 is unchanged.

## §8 Master merge preparation (master 11fa3424, spec 056, 2026-11-11)

`git merge --no-ff --no-commit master` in this worktree; nothing committed. The draft message is in the worktree's `MERGE_MSG` (no trailers).

**Conflicts.** One: `.specify/feature.json` (HEAD `specs/032-gi-cache-split`, master `specs/056-volume-shader-segments`). Resolved to 032's own value and staged. Nothing else conflicted; `tests/architecture/allowlist.txt`, `metrics-baseline.txt` and `ribvector-core-residual.txt` are untouched by master.

**Where 056 meets 032 (merged cleanly, read by hand).** Only four files changed on both sides:
- `src/libshader/shading/giFunctions.h`: 056 added `movvv(rays->sideN, N)` to the gather ray macro; 032 changed two other lines (`assert(cache == NULL || ...)`, `libshader::irradianceLookup(...)`). No overlap.
- `src/libshader/shading/rendererServices.h`: 056 corrected one comment (`worldBmin` is camera space, no version step); 032's 2.2.0 entry stands, 032 merges before 031, so it stays 2.2.0.
- `src/libshader/shading/shading.h` / `shading.cpp`: 056 added `volumeSegment`, `lightSideN`, `lightSideNStore`, `CRayBundle::continuationLimit()`, `shadeVolumeSegments()` and `CTraceLocation::sideN`; 032 added the `boundDraws()`/irradiance-sampler members and the `irradianceLookup` call in the JIT `occlusion`/`indirectdiffuse` path (shading.cpp:3714). Different members and functions.
- Not overlapping files, but adjacent in behaviour: `bundles.{h,cpp}` (`CSegmentBundle` and the trace/transmission/gather bundles), `executeMisc.cpp`, `hiders/trace.cpp` (one line: `cRay->t = bundle->continuationLimit(cRay)`). 032's irradiance sampler and the photon hider send single rays through `libshader::traceRay` (probes, no bundle), so they run no volume step; with no `Interior`/`Exterior` bound nothing changes anyway. Photon emission never calls `shade()`.

**macOS** (through cpujob, `-j4`; sweep `-j2`):
- Build (`032-p10-build`): exit 0, `-Werror`.
- Everyday suite `-LE determinism-sweep`: **1326 of 1327 passed, 100 percent of those run; 1 skipped** (`LibShader_Format_rslo_lower_minor`, "the supported .rslo minor is 0": the same skip as in `base-LastTest-macos.log`).
- Image manifest against `baseline-images-macos.sha256` (915 images): 0 missing, **2 changed**: `csg-sphere-sphere-difference-interior-raytrace` and `...-inside`, which spec 056 rewrote and re-rendered on purpose (their block-diff against the reference moved 17.27 -> 0.00 and 0.30 -> 0.00). 232 new entries: 056's volume scenes and tests, and 032's own `gi_split`/`keys` outputs recorded after the baseline. Block-diff table: 389 common rows, only those two changed.
- Architecture (`032-p10-arch`): `ctest -L architecture` **30 of 30**, `ctest -L link` **4 of 4**; `archcheck`: no STALE, no VIOLATION, `METRIC Hiders refs 549 (baseline 549) distinct 78 (baseline 78)`, `TOTAL 597 / 86` (baseline 597 / 86). The allowlist and metrics baseline survived the merge unchanged; no reconciliation needed. `libshader_shading` imports: empty, identical to after-phase 8 (`tools/libshader-imports-after-merge-macos.txt`).
- Determinism sweep `-L '^determinism-sweep$' -j2`: **582 of 582 passed**. Manifest against `baseline-sweep-images-macos.sha256` (419 entries): 0 missing, **2 changed** (the same two CSG interior scenes), 163 new (056's sweep entries, 582 - 419).
- No #163 photon-row flake, no #164 `GiKeys_PhotonsThreads` failure in this run.

**Linux (Ubuntu 24.04, GCC 13 `-Werror`, `~/Projects/Development/CLI/openrender-032`, remote cpujob `-j2`, 2026-11-12).** The host was unreachable on 2026-11-11 (no route; ICMP ping does not work through the WARP route, ssh does) and reachable the next day. `linux_sync.sh push` sent 4086 files (it removed `tests/unit/csg/test_interior_exterior_selection.cpp`, which 056 deleted), `verify` 4086 files, 0 differences; `pull` 23 files, 0 differences. Script `build-logs/032-run-p10-linux.sh`.
- Build (`032-l-p10-build`, ten `GI032_BOUNDARY_CHECKS`): exit 0, `-Werror`.
- Everyday suite `-LE determinism-sweep`: **1328 tests, 1326 passed, 1 skipped** (`LibShader_Format_rslo_lower_minor`, as on macOS), **1 failed: `TextureTile_RenderByteIdentical`, the known Linux baseline failure #45**. Nothing else red; no #163 photon row failed. `GiKeys_PhotonsThreads` passed in the suite and in its re-run.
- Image manifest against `baseline-images-linux.sha256` (915): 0 missing, **2 changed**, the same two CSG interior scenes spec 056 rewrote (block-diff 17.27 -> 0.33 and 0.30 -> 0.12, both far inside their thresholds); 234 new entries (056's scenes and tests, 032's own outputs). Block-diff table: 389 common rows, only those two changed.
- Architecture: `ctest -L architecture` **30 of 30**; `archcheck` no STALE, no VIOLATION; Hiders 549 / 78, Texturing 0 / 0, TOTAL 597 / 86, all equal to the baseline. `ctest -L link` **4 of 4**. `libshader_shading` imports (`nm -D --undefined-only`, 461 lines): identical to after Phase 8 (`tools/libshader-imports-after-merge-linux.txt`).
- Determinism sweep `-L '^determinism-sweep$' -j2`: **582 of 582 passed**; manifest against the 419-entry baseline: 0 missing, 2 changed (the same two CSG scenes), 163 new (056's sweep entries).

**Result for the merged tree:** macOS and Linux both green apart from the known #45 on Linux; the allowlist and metrics baseline survived the merge unchanged.
