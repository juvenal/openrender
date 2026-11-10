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
| T-G (`GiSplit_Boundary`) | `ctest -R` in the scratch tree | **Failed**, all ten sub-checks red: friends-texture 3, texture-names-viewer 9, hiders-no-viewer-irradiance 1, friends-hiders 1, texture-names-shading 7, texture-names-core 6, hiders-no-viewer 3, texture-includes 1, hiders-no-texture 6, files-absent 9 |

A fatal include error stops a translation unit at its first missing header, so the other missing members (`lookupCached`, `insertReceived`,
`photonKey`, `CIrradianceRecord::key`, the `getCache` kind check) are not reached; each phase finds the next. Main tree (T036): the
main `build` was reconfigured with the Phase 2 additions (option OFF) and built (`032-p2-green-build`, exit 0, nothing from `src/`
changed); `ctest -L 'gi_split|preview'` passed; `ctest -L architecture` passed (30 of 30, `GiSplit_Boundary` green with no sub-check
enabled). T035's cleanup is done: the scratch tree `build-red032` was deleted by the coordinator on the maintainer's word (2026-11-09).

T031: `GiSplit_Boundary` run on the unmodified tree with every sub-check enabled (`032-p2-boundary`, `cmake -P` of
`tests/architecture/check_gi_split_boundary.cmake`, include directories of `ribRender_obj`). What matches today, which is the red
state each phase turns green (10 sub-checks, all 10 red as expected):

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

## §7 SC-001 to SC-010 walk
