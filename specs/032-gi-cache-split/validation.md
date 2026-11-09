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

## §7 SC-001 to SC-010 walk
