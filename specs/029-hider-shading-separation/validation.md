# Validation record: spec 029, Hider / Shading Separation

Worktree `/Volumes/Projects/Development/CLI/openrender-worktrees/029-hider-shading-separation`, branch
`029-hider-shading-separation`. Base for every `git diff <base>`: master `01501f10`.

## §0 Starting-tree measurements and orientation

### T001a / T002 orientation (2026-11-02)

- T001 (PAUSE 1a) approved by the maintainer 2026-11-02; T001a done by the maintainer: `git log -1` is
  `3cddd988 Merge master (the spec 026 count fixes) into 029-hider-shading-separation`, the merge of `01501f10`.
- `git status --short` at the start: clean. Branch `029-hider-shading-separation`. `.specify/feature.json` names
  `specs/029-hider-shading-separation`.
- `git diff 01501f10 --stat -- CLAUDE.md DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md tests/architecture/allowlist.txt`
  is empty: the three master documentation files equal master `01501f10`'s.
- `git check-ignore -v build build-base build-pkg build-nojit build-logs`: all five ignored by `.gitignore:6 build-*`
  (and `build` by `.gitignore:5`).
- CLAUDE.md "Spec-kit agent rules": both copies (this worktree and the main checkout) carry the section; the two
  `CLAUDE.md` files are byte-identical (`diff` empty), same last-commit time (1793635596), no uncommitted change. The
  worktree copy was followed; the rules are identical.
- Disk: `df -h /Volumes/Projects` 8.4 GiB free at the start.

### macOS baseline (T007 to T012; starting `src/`, Release, JIT on, vcpkg shared tree, `-Werror`)

Environment: `VCPKG_ROOT=/Users/juvenal/.vcpkg`, `VCPKG_INSTALLED_DIR=/Users/juvenal/.cache/vcpkg-installed/openrender`
(the configure reused the binary cache; LLVM 23.1.1 core-only).

| Task | Result |
|---|---|
| T007 configure (`029-base-configure`) | clean |
| T007 build (`029-base-build`, `-j4`) | exit 0, no warning, every target built (`orender-wire-macos` included) |
| T008 suite (`029-base-suite`, `-LE determinism-sweep -j4`) | **1052 tests, 100% passed**; 1 test skips itself on the baseline (`LibShader_Format_rslo_lower_minor`, `Skipped`). Real time 272 s. No failure. `baseline-images-macos.sha256`: **900 images**; `baseline-diffs-macos.txt`: 389 tests with a `MaxBlockAvgDiff` |
| T009 noise (`029-base-suite2`) | run 2: 1052 tests, 100% passed, same skip; **900 images, 0 differing, 0 missing**; every `MaxBlockAvgDiff` equal (the two listings are identical). `baseline-image-noise-macos.txt` is empty (comment only) |
| T010 archcheck (`029-base-archreport`) | `ALLOWED DB-001 x 5, DB-004 x 52, DB-005 x 8, DB-006 x 31, DB-010 x 8, DB-011 x 166, DB-015 x 10, DB-017 x 78, DB-025 x 12`; `METRIC TOTAL refs 612 distinct 94`, Shading 0; `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`. Equal to the expected 5 / 8 / 31 / 166 |
| T010 imports | exactly **4** (`nm -m ... | grep 'dynamically looked up'`): `CPhotonMap::lookup(float*, float const*, int)`, `CGatherBundle::CGatherBundle()`, `vtable for CTraceBundle`, `vtable for CTransmissionBundle`. `libshader-fixups-base-macos.txt` has 39,588 lines |
| T011 sweep (`029-base-sweep`, `-L '^determinism-sweep$' -j2`) | **419 of 419 passed**, 611 s; 419 images in `baseline-sweep-images-macos.sha256` (the sweep leaves its images under `build/tests`, so the manifest mechanism applies as it is) |
| T011 sweep noise (`029-base-sweep2`) | run 2: 419 of 419 passed (596 s); **0 differing**; `baseline-sweep-noise-macos.txt` is empty |
| T012 frozen copy | `cp -Rp build build-base` (575 MB) under `cpujob 029-freeze-base`, then install names, `LC_RPATH` entries and dylib ids rewritten with `install_name_tool` and every modified Mach-O re-signed (`codesign -f -s -`): **307 Mach-O files seen, 199 rewritten, 0 still naming `$W/build/` in `otool -L` or `LC_RPATH`**. Run check (`029-base-frozen2`): one `orender` run of `sphere-gather-reyes` from `build-base` with `DYLD_PRINT_LIBRARIES=1` loads 6 openRender images (`orender`, `libRIBRender`, `libRIBCommon`, `libshader_shading`, `libRIBShadeop`, `file.dsply`), **all from `build-base/`, none from `build/`** |
| T012 `timing_sc010.sh` (the Phase 1 version; the scaling of §4 F1 was added after review) | copied from 026's, `SCENES` set to research R14's (`teapot-somewood-reyes teapot-wood-reyes teapot-blue_marble-reyes sphere-gather-reyes photon-parity killeroo-reyes`; `photon-parity` runs at `-t:1`, the photon hider races), environment built from the tree being run, stale `.gpm` removed before each run, `SCENES` / `SCENE_THREADS` overridable. One sanity series (`029-sc010-sanity`, `build-base` vs `build`, expected equal): **every scene renders in 0.09 to 0.80 s**; see §4 finding F1 |

### T013: does a suite photon scene's light shader trace during photon emission? (research R10 item 3)

Statically: **no.** The photon-mode scenes are `photon-parity{,-slo}`, `photon-solar0-{alone,after-distant,before-distant}{,-slo}`
(through `Visual_photon-solar0-*` and `PhotonSolar0_*`), and the four `subdiv-*-photon` scenes. The lights they bind are
`pointlight`, `spotlight`, `distantlight`, `ambientlight` and `solar0_light` (`shaders/*.sl`); none contains a call to
`trace`, `transmission`, `visibility`, `shadow`, `environment`, `occlusion` or `indirectdiffuse`, and none has an `#include`.
(`photonparity_view.sl` and `photonmap_probe.sl` call `photonmap()` in the camera frame, not in a light.) So no suite scene
exercises the corrected 0-D/`inShadow` branch of `classifyShading()` through a light during photon emission; T-G1's dynamic
confirmation (Phase 7) is the first exercise. The correction is made either way.

### T006: the measured test-edit inventory (`tools/test-edit-inventory.txt`, HEAD `3cddd988`)

Measured with `grep -rnE` over `tests/` and `src/` (C/C++ sources, headers, CMake files). **Every list of Phases 3, 5 and 7
matches the measurement; no difference.** Counts per pattern: subclasses of `CShadingContext` 12 (3 hiders + 9 test classes in 9
files); `CRenderer::contexts` 21 occurrences (11 test reads in 9 files, matching Phase 5's list B, plus two comment mentions in
`tests/imager/test_imager_execution.cpp` and one in `csgTestUtils.h`; the rest is `src`); `CRenderer::activeContext` 5 (all
`src`, no test); `currentObject` writes in tests 9 in 8 files (Phase 7's five of list A and four of list B; two more test files
mention it in comments only, `case_null_object{,_messages}.cpp:7`, and no edit is listed for them); `getPhotonMap` doubles 6
(exactly the six of T044); `CHiderTracer`/`frameTracer` in tests 0; `->trace(`/`traceEx` in tests 0 (a comment in
`load_stub.cpp`, which is deleted); `CPointKeyReport`/`CPointDisplacedReport` users dereferencing `->surface`: `scene_probe.h:74-75`
and `test_displace_consistency.cpp:55` (the conditional include-path edits of Phase 7); hook overrides in tests: the four probes
(`illuminateBegin` 4x, `solarBegin` 3x); includes in tests: `bundles.h` only in `load_stub.cpp` (deleted), `photonMap.h` in four
test files (`tests/preview/test_data_world_init.cpp`, `test_dataview_chunking.cpp`, `tests/unit/photon_map/test_photon_map_*.cpp`),
`irradiance.h` none. The four `photonMap.h` includers are not in any list and need no edit: the header stays in
`src/ri/hiders`, and `photonLookup.h` (T048) is reachable for them because their targets link `ribRender`, whose PUBLIC include
directories carry `src/libshader/shading` (`src/ri/CMakeLists.txt:229-242`). Phase 3's build confirms it.
Source-side observation (not a test edit): `src/ri/render/imager.cpp:130-134` reads `CRenderer::contexts[0]` into a
`CShadingContext *` and `rendererJobs.cpp:88-89` / `rendererDeclarationsDispatch.cpp:82` call members through `contexts[i]`;
research and plan name all three files already (plan.md:72, research.md:121-122).

### macOS red evidence (T014, T015)

- **T014 red link** (`029-red-link-macos`, `tools/red-link-macos.txt`): `libshader_shading`'s generated `link.txt` run from its own directory with
  `-undefined dynamic_lookup` removed and `-o` pointed at a scratch path: exit 1, **exactly the 4 symbols** of T010
  (`CPhotonMap::lookup(float*, float const*, int)`, `CGatherBundle::CGatherBundle()`, `vtable for CTraceBundle`,
  `vtable for CTransmissionBundle`); no other undefined symbol. No finding.
- **T015 package consumer** (`tools/red-package-macos.txt`): scratch install (`build-pkg/prefix`, a full second build and install, exit 0
  with `CPUJOB_TIMEOUT=7200`), scratch consumer under `build-pkg/consumer/`. (a) as shipped: image written (5,273 bytes), exit 0.
  (b) the `-force_load` block deleted from a scratch copy of the installed config: configure and link succeed, **launch abort**
  `dyld: symbol not found in flat namespace '__ZTV12CTraceBundle'`, exit 134. FR-004's red reproduces on macOS.
- **T015 (c), P-Q17 backend measurement** (`tools/red-backend-macos.txt`): (b)'s prefix plus a `refs.cpp` that references only the four
  imports, linked with `-Wl,-why_load`: **`ribOut.cpp.o` and `rendererContext.cpp.o` ARE pulled** (`ribOut.cpp.o` by `preferCompressedRibOut`,
  referenced directly by `ri.cpp.o`; `rendererContext.cpp.o` by `CRendererContext::getXform(int)`, referenced by
  `rendererDeclarationsDispatch.cpp.o`), 96 members of `libRIBRender.a` in all; the consumer's `RiBegin` reports no `CODE_INCAPABLE` and the image is
  written. **P-Q17 does not stop the work on macOS: there is nothing to decide today.** The registration works through ordinary
  transitive references of the starting tree, not through any reference to the registrars; T054 repeats the measurement after Phase 3.

## §3 Linux records

Tree `juvenal@ubuntu-2404-desktop.v2-labs.io:~/Projects/Development/CLI/openrender-029` (created in T016; `openrender`, `openrender-038`, `cellar` and
`shrimp` untouched). GCC 13.3.0, cmake 3.28.3, LLVM 18, 2 cores, Release, JIT on. Every remote job through the remote `cpujob` at `-j2`.

| Step | Result |
|---|---|
| T016 `linux_sync.sh push` / `verify` | pushed 3,505 files; **3505 files, 0 differences**; `no-git-ok` (no `.git` in the remote tree). The first push created the tree (checked beforehand: no `openrender-029` existed) |
| T016 configure (`029-l1-configure`) | clean |
| T017 build (`029-l1-build`, `-j2`) | exit 0, **0 warnings** under GCC 13 `-Werror` (the default 2700 s timeout was enough) |
| T018 `ctest -L architecture` | 21 of 21 passed. `archcheck-base-linux.txt`: `ALLOWED DB-001 x 5 ... DB-005 x 8 ... DB-006 x 31 ... DB-011 x 166`, `RESULT PASS` (equal to macOS). `nm -D --undefined-only libshader_shading.so`: **465 undefined symbols, the 4 among them** (`CPhotonMap::lookup`, `CGatherBundle::CGatherBundle()`, `vtable for CTraceBundle`, `vtable for CTransmissionBundle`). `readelf -d`: no `FLAGS` entry, so **no `BIND_NOW`** (lazy binding); `NEEDED` libRIBShadeop, libRIBCommon, libz, libzstd, libtinfo, libstdc++, libm, libgcc_s, libc |
| T019 suite 1 (`029-l1-suite`, `-LE determinism-sweep -j2`) | **1053 tests, 1046 passed, 7 failed** (680 s): `CoverageAnalytic_alpha-sides2-tjunction-r4_reyes` (#57), `TextureTile_RenderByteIdentical` (#45), `TextureTile_CacheByteIdentical`, `TextureTile_CacheDefaultDirectory`, `TextureTile_CacheStaleDetection`, `TextureTile_CacheConcurrentWrite` (the four `TextureTile_Cache*` of #46), `LibShader_McompConsistency` (#44). **Exactly the recorded 7.** 1 test skips itself (`LibShader_Format_rslo_lower_minor`). Linux has one test macOS lacks (`IPCDisplayTests`). 900 images (the same count as macOS) |
| T019 suite 2 (noise) | the same 7 failures; **900 images, 0 differing**; the `MaxBlockAvgDiff` listings are identical (diff empty); `baseline-image-noise-linux.txt` is empty (comment only) |
| T020 sweep 1 (`029-l1-sweep`, `-j2`) | **419 of 419 passed** (1,387 s); 419 images in `baseline-sweep-images-linux.sha256` |
| T020 sweep 2 | 418 of 419: **`DeterminismSweep_sphere-texture3d-bake3d-reyes` failed (orender segmentation fault in the default-threads configuration)**; 1 image differs from run 1 (`sphere-texture3d-bake3d-reyes.tif`). Spec 026 recorded this bake3d scene as flaky on the Linux starting tree; `baseline-sweep-noise-linux.txt` lists that image. Nothing else differs |
| T021 red link (`029-red-link-linux`) | `libshader_shading`'s `link.txt` with `-Wl,--no-undefined` added and `-o` redirected: exit 1, **exactly the 4 symbols** (`CPhotonMap::lookup(float*, float const*, int)` at several sites, `CGatherBundle::CGatherBundle()`, `vtable for CTransmissionBundle`, `vtable for CTraceBundle`); none of the other 461 undefined symbols. No fifth symbol: no finding |
| T022 package consumer (remote `build-pkg`, scratch prefix, `CPUJOB_TIMEOUT=7200`; configure, build and install exit 0) | (a) as shipped: image written, exit 0. (b) the `--whole-archive` / `--export-dynamic` block deleted from a scratch copy of the installed config: the consumer **links** (the package's static targets put `--allow-shlib-undefined` on every consumer link) and **aborts at launch**: `libshader_shading.so.1: undefined symbol: _ZTV19CTransmissionBundle`, exit 127. FR-004's red reproduces on Linux (`tools/red-package-linux.txt`) |
| T022 (c), P-Q17 backend measurement | `-Wl,-Map=` of (b)'s prefix plus `refs.cpp`: `ribOut.cpp.o` (by `preferCompressedRibOut` from `ri.cpp.o`) and `rendererContext.cpp.o` (by `CRendererContext::getXform(int)` from `rendererDeclarationsDispatch.cpp.o`) **are included**, 200 `libRIBRender.a` mentions in the map; the image is written, no `CODE_INCAPABLE`. Same chain as macOS (`tools/red-backend-linux.txt`) |
| T022 cleanup | the remote `build-pkg` deleted (`~/` has 9.7 GB free) |
| T023 `linux_sync.sh pull` / `verify` | the 13 Linux-produced files pulled (13 files, 0 differences), all 12 files T023 names exist locally (plus `red-backend-linux.txt`); after `git add`, **`verify`: 3517 files, 0 differences**, no remote `.git` |

SC-009's Linux comparison basis (§4a): the failing set of the starting tree is the 7 above; the sweep has no failure in run 1 and the one flaky bake3d scene in run 2.

## §1 Checkpoint records

Checkpoint 1 (Phase 1; nothing in `src/` changed): the baselines above are the checkpoint. `ALLOWED` per platform: DB-001 5, DB-005 8, DB-006 31, DB-011 166
(macOS and Linux), `RESULT PASS`; 4 imports on both platforms.

## §2 Red evidence

Phase 1's red evidence is the two red links and the two package-consumer reds recorded above; Phase 2's is in the section "Phase 2" below.

## §4 Findings for filing as P1, and departures from the plan

- **F1 (SC-010 scene set is too short to time). Decision (maintainer, 2026-11-02): time scaled scratch copies; see the resolution below.** Research R14's six scenes (`teapot-somewood-reyes`, `teapot-wood-reyes`,
  `teapot-blue_marble-reyes`, `sphere-gather-reyes`, `photon-parity`, `killeroo-reyes`) render in 0.09 to 0.80 s each at `-t:4`
  (sanity series `029-sc010-sanity`, `build-base` vs `build`, which are the same bits; the script's "BEYOND SPREAD" lines for
  `photon-parity` and `killeroo-reyes` are an artefact of one series: the spread of a single run is 0). A difference of a few
  milliseconds is process start-up and thread-pool noise, so SC-010 as specified cannot see a per-batch cost of the new virtual calls. The
  heaviest scenes of the whole suite (`Convergence_dof-reyes` 23 s, `Convergence_motion-reyes` 15 s, `Visual_teapot-motion-raytrace` 33 s,
  `Parity_csg-sphere-cube-difference-1600-*` 13 s) are not shading-bound in the sense of R14. For the maintainer to decide before Phase 8:
  heavier scenes, or scratch copies of the R14 RIBs with a larger `Format`/`PixelSamples`/finer `ShadingRate` (the script's `SCENES`
  and the RIB directory are overridable). Not a defect of the code; a gap in the plan's measurement.
  **Resolution (maintainer, 2026-11-02):** `timing_sc010.sh` generates scratch copies of the six RIBs (never tracked scenes) with a larger `Format`,
  a finer `ShadingRate` and, for `photon-parity`, more photons, until each render takes several seconds at `-t:4`. Initial factors (an estimate from the
  unscaled times and the cost model, **not yet timed**): `teapot-somewood-reyes` Format x2 ShadingRate /2; `teapot-wood-reyes` x4 /4;
  `teapot-blue_marble-reyes` x4 /4; `sphere-gather-reyes` x3 /2; `photon-parity` Format x6, emit x4 (`-t:1`); `killeroo-reyes` x2 /4. The script flags `TOO SHORT`
  (median under `MIN_SECONDS`, default 3 s; exit 3). The confirmed factors and medians are recorded in §5 when the first series runs. The change to the approved
  `timing_sc010.sh` is the one reported at the Phase 1 hand-back; research R14, T012, T114 and quickstart.md §0/§5 were updated.
- **F2 resolved by the maintainer (departure: `.gitignore` edited on this branch).** `.gitignore` gained `!specs/029-hider-shading-separation/tools/*.log` after
  `*.log`, and the four `base-*LastTest-*.log` files (about 5 MB) are staged, so all 30 approved references are tracked. Original finding:
- **F2 (the `.log` references are gitignored).** `.gitignore:67` is `*.log`, so `tools/base-LastTest-{macos,linux}.log` and
  `tools/base-sweep-LastTest-{macos,linux}.log` (T001's list; 1.0 to 1.7 MB each) exist on disk but `git add` skips them; the other 26 references are staged.
  Spec 026's tools directory has none of them either. `-f` would stage them; the CP-d checks use `baseline-diffs-*.txt` and the manifests, not the logs.
- **F3 = GitHub #141 (P1, filed by the maintainer 2026-11-02).** The coordinator verified it: both trees use the one shared vcpkg install, and a consumer
  with `CMAKE_PREFIX_PATH` at the prefix plus the shared `arm64-osx-openrender` dir, without the toolchain, still fails (31 undefined: 25 jpeg/jpeg12, 6 lzma, from `libtiff.a`);
  the prefix alone picks Homebrew TIFF/PNG and fails on 14 OpenEXR `Imf_3_4` symbols. T-L4 (T030, T115, `contracts/link-and-tests.md`) now states that its macOS consumer is configured
  with the vcpkg toolchain line from the main build's cache. The coordinator's scratch builds `build-pkg/consumer/build-{d,e}` go when `build-pkg` is removed at T040. Original finding:
- **F3 (the installed Static component is not self-sufficient on a vcpkg macOS install).** A consumer that only sets
  `CMAKE_PREFIX_PATH` to the install prefix and the vcpkg installed tree fails to link (`_jpeg_abort`, `_jpeg_std_error` from the static
  `libtiff.a`): CMake's `FindTIFF` module does not carry libtiff's own dependencies. It links when the consumer is configured with vcpkg's
  toolchain (`CMAKE_TOOLCHAIN_FILE`, `VCPKG_MANIFEST_MODE=OFF`, `VCPKG_INSTALLED_DIR`, `VCPKG_TARGET_TRIPLET`), which is how the scratch consumer ran. T-L4
  (`Link_PackageStaticConsumer`, Phase 8) needs the same configure line on macOS. A property of the existing package, not of 029.
- **F4 (decision: stays).** The Linux bake3d sweep flake stays in `baseline-sweep-noise-linux.txt` as recorded. Original finding:
- **F4 (known flake, restated).** Linux sweep run 2: `DeterminismSweep_sphere-texture3d-bake3d-reyes` segfaulted once (default threads); it
  passed in run 1 and is flaky on the starting tree per spec 026's record. The macOS sweep had no such failure in two runs.
- **F5 (P-Q17 does not stop Phase 2).** See the macOS and Linux measurements above: a plain static link registers the backend
  today, through transitive references. It is a property of the starting tree's archive-member graph (nothing names the registrars), so Phase 3
  (T054) and Phase 4 re-measure it; if a later phase's move of `bundles.cpp` ever changes which members are pulled, the maintainer's options in plan P-Q17 apply then.
- Departures from the plan: none in the test-edit lists (T006). The frozen copy's binaries were re-signed ad hoc after `install_name_tool`
  (the task says so). The Linux first build and suites needed no extended timeout.

## §5 Final summary against SC-001 to SC-011

Not applicable before Phase 8. SC-010's scaled scenes and the confirmed factors are recorded here when T114 runs.

## Phase 2 (tests first; no `src/` edit)

Approved by the maintainer 2026-11-02 (T025): the CMake edits of `tests/CMakeLists.txt` and `tests/shading_contract/CMakeLists.txt`, the two options, the patch (written, not applied), the new tests and the three scenes. `git diff 01501f10 -- src` is empty; no approved test source and no approved reference is edited (`git diff -- tests/shading_contract/case_situations.cpp` is empty after every evidence run).

### What was added

- Options (`tests/CMakeLists.txt`): `OPENRENDER_029_RED_TESTS` (OFF, temporary, removed in Phase 8) and `OPENRENDER_LINK_PACKAGE_TEST` (OFF, permanent, documented); `add_subdirectory(link)`.
- `tests/link/`: `CMakeLists.txt`, `check_shading_self_contained.cmake` (T-L1), `static_consumer.cpp` + `run_static_consumer.cmake` (T-L2), `check_no_workaround_text.cmake` (T-L3), `package/{CMakeLists.txt,main.cpp}` + `run_package_test.cmake` (T-L4), `check_scope.cmake` (T-S1, sub-checks S1a to S1f).
- `tests/shading_contract/`: eight programs, one executable each, defined only inside `if(OPENRENDER_029_RED_TESTS)`: `test_shading_contract_standalone_context` (T-S2), `_photon_lookup` (T-P1), `_canned_bundles` (T-B1), `_hook_interface` (T-H1), `_ray_level` (T-T2), `_point_source` (T-G2), `_hider_tracer` (T-T1, links ribRender), `_producer_situations` (T-G1, links ribRender); `cases_029.h`; `photon_services.h`; six shaders (`cm_standalone`, `cm_photon`, `cm_hook_{illuminate1,illuminate3,solar2,solar0}`, compiled as build steps only with the option ON); three scenes (`sit_row4_secondary_raster_{shaded,primitive}.rib`, `sit_row5_shadow_implicit_primitive.rib`). The 13 contract tests and the 4 link tests are registered `DISABLED` while the option is OFF ("Not Run (Disabled)").
- `tools/situations-q5.patch` (T-G3, the P-Q5 edit, not applied) and `tools/situations-parse-fix.patch` (proposed, see F6).
- `SHADING_CONTRACT_ENV` gained `SHADING_CONTRACT_IMPLICIT_DIR` (the directory of the implicit fixture `good.dsply`); the patch and T-G1 read it.

### §2 Red evidence, macOS (`build` configured with `-DOPENRENDER_029_RED_TESTS=ON`; one target at a time through `cpujob`)

| Test | Result | Reason |
|---|---|---|
| T-S2 `test_shading_contract_standalone_context` | build failed | `static assertion failed: CShadingContext has no virtual function`, `... is final`, `allocating an object of abstract class type 'CShadingContext'`, no member `illuminationHook` |
| T-P1 `..._photon_lookup` | build failed | fatal: `libshader/shading/photonLookup.h` file not found |
| T-B1 `..._canned_bundles` | build failed | fatal: `libshader/shading/bundles.h` file not found (the header is `src/ri/hiders/bundles.h`; included by its post-move path, so the red says something about the bundles) |
| T-H1 `..._hook_interface` | build failed | fatal: `libshader/shading/illuminationHook.h` file not found |
| T-T2 `..._ray_level` | build failed | no member `enterRayLevel`, `leaveRayLevel`, `rayDepth`; unknown type name for the ray-level type |
| T-G2 `..._point_source` | build failed | fatal: `libshader/shading/pointSource.h` file not found |
| T-T1 `..._hider_tracer` | build failed | fatal: `hider.h` file not found |
| T-G1 `..._producer_situations` | build failed | unknown type `CShadeBatchReport`; no member `shadeBatchObserver`; `SHADEBRANCH_*` undeclared; no `BatchShape::PointSet` |
| T-L2 `test_link_static_consumer` | builds; `Link_StaticConsumer` fails | both formats (`rslo`, `slo`): `dyld: symbol not found in flat namespace '__ZTV12CTraceBundle'` (Subprocess aborted) |
| T-L1 `Link_ShadingSelfContained` | fails | the 4 imports: `CPhotonMap::lookup`, `CGatherBundle::CGatherBundle()`, `vtable for CTraceBundle`, `vtable for CTransmissionBundle` (each reported by `dyld_info -imports` and by `nm -m`) |
| T-L3 `Link_NoWorkaroundText` | fails | 18 matches: `CMake/openRenderConfig.cmake.in:81,91,95,96`; `src/libshader/shading/CMakeLists.txt:56,85,92,100`; `tests/unit/common/CMakeLists.txt:79,82,83`; `tests/shading_parity/CMakeLists.txt:326,346,368,389`; the load stub exists |
| T-S1 registered `ShadingScope_Greps` | fails | `CHECKS is empty` (an empty list is an error, so it cannot pass vacuously): expected with the option ON in Phase 2 |
| T-S1 `cmake -DCHECKS='S1a;S1b;S1c;S1d;S1e;S1f' -P tests/link/check_scope.cmake` | each of the six fails | S1a 8 (bundles.h x4, irradiance.h x2, photonMap.h x2 included from `src/libshader`); S1b 6; S1c 22 (3 virtuals, 3 friends, not final, 10 subclasses of `CShadingContext` in `src` and `tests`); S1d 19 (4 trace members, the CHiderTracer/frameTracer text, `hiderTracer.{h,cpp}`); S1e 2 (`shading.cpp` includes `object.h`); S1f 3 (the three `friend class`) |
| T-L4 runner against T015's install (`-DPREFIX_DIR=<build-pkg/prefix>`, vcpkg toolchain line from `build/CMakeCache.txt`, GitHub #141) | as shipped: both formats pass (image written). `-DREMOVE_PACKAGE_BLOCK=ON`: both formats fail with `dyld: symbol not found in flat namespace '__ZTV12CTraceBundle'`, so the runner exits 0 ("every format fails, as the red mechanism requires") | the consumer passes only because of the package block |
| T-G3 `tools/situations-q5.patch` applied; `test_shading_contract_situations` rebuilt; `ShadingContract_Situations` | **red**: 310 passed, 1 failed: `c2.rayType == Shadow && c2.hitMode == Primitive` (row 6's shadow case, `classifyShading()` returns Shade for a 0-D shadow hit today). With `situations-parse-fix.patch` on top (F6): 293 passed, **18 failed**: that one plus 17 x `row 4: classified None/0/0/0, expected Secondary/0/0/0` (the reflection hits of the REYES frame `sit_row4_secondary_raster_shaded`, which today's function sends to row 1) | P-Q5's corrected expectations against today's `classifyShading()`. Reverted with `git apply -R`; `git diff` of the file is empty |

Every other row of the corrected table, applied to the real reports, holds for the ten scenes of 026 (with the parse fix): the correction changes only the REYES secondary hits and the 0-D shadow case. The "build failed" rows show the first error of each file only; see "What the red builds do not prove" below.

### §3 Red evidence, Linux (GCC 13.3, `-Werror`, `-j2`, remote `cpujob`; `build` configured ON, then OFF again)

The eight programs fail to build for the same reasons (GCC wording: `static assertion failed`, `invalid new-expression of abstract class type`, `fatal error: ... No such file or directory`, `'enterRayLevel' is not a member of 'CShadingContext'`, `'CShadeBatchReport' does not name a type`). `test_link_static_consumer` builds. `Link_StaticConsumer`: `symbol lookup error: libshader_shading.so.1: undefined symbol` at launch, exit 127, both formats. `Link_ShadingSelfContained`: `imports a symbol libRIBRender defines` for exactly the 4 symbols. `Link_NoWorkaroundText` and the six-sub-check `check_scope.cmake` fail as on macOS (S1a 8, S1b 6, S1c 22, S1d 19, S1e 2, S1f 3 failure lines).

### What the red builds do not prove (and the extra check made)

A test that stops at a missing header was compiled by neither compiler past that line, so the red builds say nothing about GCC accepting the rest. Extra check, in the scratchpad only (nothing tracked): a header overlay with the interfaces exactly as the contracts declare them (`photonLookup.h`, `illuminationHook.h`, `pointSource.h`, `shadingProducer.h`, a copy of `shading.h` made `final` and non-polymorphic with the new members, `BatchShape::PointSet`, `hider.h`/`raytracer.h`/`photon.h` stubs, `getPhotonMap` of the two doubles on `IPhotonLookup *`, `CHarnessContext` as an alias of `CShadingContext`) and `-fsyntax-only` of the eight case files with each target's own flags (`-Wall -Wextra -Wpedantic ... -Werror`): **all eight compile with no diagnostic under Apple clang and under GCC 13.3** (T-S2, T-P1, T-B1, T-H1, T-T2, T-G2, T-T1, T-G1). The check found one real bug in my test (`case_ray_level.cpp` used `CRay` without `ray.h`), fixed. What it cannot show: that the contracts' signatures are the ones Phases 3 to 7 land (see the decisions below).

### §1 Phase 2 checkpoint (T042; CP-a to CP-d, CP-m, CP-v)

| | macOS | Linux (GCC 13.3, `-Werror`, `-j2`) |
|---|---|---|
| Option | configured back to `-DOPENRENDER_029_RED_TESTS=OFF` | same |
| CP-a build | exit 0, no warning (the new targets are not in the build; `test_shading_contract_situations` was rebuilt from the unmodified source) | exit 0, 0 warnings |
| CP-b suite (`-LE determinism-sweep`) | **1069 tests: 1052 passed, 17 "Not Run (Disabled)"** (13 contract + 4 link, the new entries), 1 self-skip as in the baseline; no failure | 1070 tests: **the baseline's failing set** (`CoverageAnalytic_alpha-sides2-tjunction-r4_reyes`, `TextureTile_RenderByteIdentical`, `TextureTile_Cache{ByteIdentical,DefaultDirectory,StaleDetection,ConcurrentWrite}`, `LibShader_McompConsistency`: 7 of 1053 as before), 17 new entries Not Run; 692 s |
| CP-d images | `image_manifest.sh diff`: 900 images, **0 differing, 0 missing**; every `MaxBlockAvgDiff` equal (the diffs listing equals the baseline's) | 900 images, **0 differing, 0 missing**; diffs listing equal |
| CP-c architecture | `ctest -L architecture` 21 of 21; `ALLOWED DB-001 x 5, DB-005 x 8, DB-006 x 31, DB-011 x 166`, `RESULT PASS`, `METRIC TOTAL refs 612 (baseline 612)` | 21 of 21; the same `ALLOWED`, `RESULT PASS` |
| CP-m (`-L '^shading_contract$\|^link$'`) | 55 of 55 pass, 17 Not Run (Disabled), the replay green; `git diff --stat` of `tests/shading_contract/replay` and of `tools/baseline*` empty | 55 pass, 17 Not Run |
| CP-v | no `static_assert` line of the six files of `renderer-services-version.md` is edited | same tree |

No label added contains the substring `architecture` (`shading_contract`, `link`, `link-package`). The Linux tree was synced with `linux_sync.sh push`/`verify` (3,557 files, 0 differences); `build-logs/overlay` on the remote is scratch (not pushed, not tracked).

### §4 Phase 2 findings and decisions

**Decisions the written tests need from the maintainer at the T043 approval (proposed contract amendments; none is in the contracts):**

- **D1 `shadeBatchObserver` and its report.** The contracts name the observer and not its report. T-G1 is written to this shape (shading.h, beside `CPointKeyReport`): `enum EShadeBranch { SHADEBRANCH_FULL, SHADEBRANCH_DISPLACE_ONLY, SHADEBRANCH_DISPLACE_ESTIMATE, SHADEBRANCH_SHADOW_PRIMITIVE, SHADEBRANCH_SPECULAR_PRIMITIVE, SHADEBRANCH_CAMERA_PRIMITIVE }`; `struct CShadeBatchReport { const CSurface *surface; const openrender::shading::ShaderGlobals *globals; EShadingDim dim; int displaceOnly; int inShadow; int rayDepth; int illuminationHook; EShadeBranch branch; }`; `typedef void (*CShadeBatchObserver)(const CShadeBatchReport &)`; `static CShadeBatchObserver CShadingContext::shadeBatchObserver`, null in production, called once per `shade()` call after the batch is described and before the branch runs (the primitive shortcuts and the displacement passes included). The raw situation facts (`dim`, `displaceOnly`, `inShadow`, `rayDepth`, `illuminationHook`) are in the report, so the expectation never needs `classifyShading()`.
- **D2 a read of the per-context traced-ray count.** The contracts give `countTracedRay()` and no getter. T-T1 uses `int CShadingContext::tracedRays() const` (read-only, like `vertexMemoryInUse()`).
- **D3 where `CRayLevel` lives.** `shading-context.md` lists it among the context's members; `hider.md` uses it unqualified from a class that is not the context. T-T2 names the type through `decltype` of `enterRayLevel`'s third parameter, so either placement compiles, and reads the ray label back through the next level's `savedLabel` (the context has no public label accessor).
- **D4 phase fit.** T-S2 is enabled in Phase 5, so it does not touch the tracer accessor (`tracer()` arrives in Phase 6; T-T1 checks it). T-P1 and T-B1 (Phase 3) and T-H1 (Phase 5) use only what their own phase adds.
- **D5 `photon_services.h`.** The services double is `final` and answers no photon map, so T-P1 wraps it and points the state's `services` field at the wrapper after the harness built the context. This relies on `CShadingState::services` staying a public field and on the double's `getPhotonMap` becoming `IPhotonLookup *` in Phase 3 (already in the TEST-APPROVAL list).
- **D6 T-G1's implicit scene.** See F7: the scene's own `(Shadow, Primitive, PointSet)` batch is not required.
- **D7 P-Q5's patch and the parse bug.** See F6: the maintainer decides whether `situations-parse-fix.patch` joins T-G3 at the Phase 7 approval.

**Findings (out of the plan; reported, not fixed):**

- **F6 (P1 candidate; a defect of an approved test, spec 026).** `ShadingContract_Situations` parses the key observer's lines with `sscanf(line.c_str() + 7, "dim=%d ...")`. A key line is `R key dim=...`, where `dim=` starts at offset 6, so the format never matches and **every key report is read as all zeros** (dim 0, depth 0, no shadow, no hook). Only the `R disp` lines (offset 7) parse. So the 026 test's per-report checks are vacuous for the shaded-grid and ray-hit reports, and the REYES secondary hits that P-Q5 corrects are invisible to it. `tools/situations-parse-fix.patch` (one line: `sscanf(strstr(line.c_str(), "dim="), ...)`) applies on top of `situations-q5.patch`. Measured: without it the P-Q5 patch goes red on one assertion (row 6's classify-only shadow case); with it, on 18 (that one plus 17 x `row 4: classified None/0/0/0, expected Secondary/0/0/0` on the new REYES scene), and the ten scenes of 026 all pass against the corrected table. Recommendation: include the one-line fix in T-G3 at the Phase 7 approval. T-G1 has its own parser and is not affected.
- **F7 (the implicit fixture draws nothing under the raytrace hider). Filed by the maintainer as GitHub #144, P1 ("Implicit surfaces are evaluated in camera space, so they draw nothing under raytrace"). Cause: `CImplicit::intersect` (`src/ri/geometry/implicitSurface.cpp:122-140`) passes camera-space points to `implicitEval` and never transforms the ray into object space, as `CDLObject` does with `transformRay`. Out of spec: not fixed here; D6 stands.** `tests/plugins/implicit` (`good.dsply`, a unit-sphere field) loads (a wrong name is reported) and `implicitEval` is called (758 times for a 16 x 16 frame), but the image equals the empty scene's (same md5) and a shader of the object never runs (no marker, as a camera hit or as a shadow occluder under `transmissionhitmode "shader"`). The plan's "implicit-surface shadow hit" therefore cannot be shown with an existing fixture, and I wrote no new plug-in. `sit_row5_shadow_implicit_primitive.rib` is in place and renders; `ShadingContract_ProducerSituations` checks every batch it does produce but does not require the 0-D shadow batch. Options: (a) another implicit fixture, (b) accept the weaker scene, (c) look at `CImplicit::intersect` (a possible defect; not investigated beyond the measurement).
- **F8 (`dyld_info -fixups` is unusable here).** On this machine (Xcode 27) `dyld_info -fixups libshader_shading` exits 133 after a truncated listing (39,724 lines in one run, 39,587 in another). `-imports` exits 0 and lists the same flat-namespace imports (`(from <flat-namespace>)`). T-L1 uses `dyld_info -imports` and `nm -m` instead of `-fixups` (FR-001's wording). The Phase 1 record `libshader-fixups-base-macos.txt` may therefore be truncated; nothing reads it.
- **F9 (T-L3 and T-S1 matches and the tasks that remove them).** T-L3's 18 matches: the package block (`openRenderConfig.cmake.in:81,91,95,96`) goes with the Phase 4 deletion; `src/libshader/shading/CMakeLists.txt:85,92,100` with the replaced link options; **`:56` (the `libshader_jitmeta` comment that names `dynamic_lookup`) only if the rewrite of `:50-57` drops that word**; `tests/unit/common/CMakeLists.txt:79,82,83` with T062; `tests/shading_parity/CMakeLists.txt:326,346,368,389` with Phase 4; `load_stub.cpp` with T052. T-S1: S1a/S1b by T046/T047, S1c/S1f by T071/T072, S1d by T087/T090 and the `tracer.h` comment rewrite that `renderer-services-version.md` already lists, S1e by T101. Nothing is left that no task removes.
- **F10 (T-L2 on Linux).** The plain static consumer fails at launch with `symbol lookup error ... undefined symbol` (exit 127), as T022 measured; macOS aborts in dyld.

**Departures from the plan / tasks:**

- the P-Q5 edit is a patch applied in Phase 7, not an in-place edit (DEP-1, approved); `situations-parse-fix.patch` is new (F6);
- the gating option and the opt-in option (approved);
- the new block of `tests/shading_contract/CMakeLists.txt` is at the end of the file (after the Situations registration), not "after :191"; it defines the functions `or029_contract_program` and `add_contract_test_029`; the shared `SHADING_CONTRACT_ENV` gained `SHADING_CONTRACT_IMPLICIT_DIR` (the existing tests see one extra environment variable);
- T-L1 on macOS uses `dyld_info -imports` (F8);
- the six new shaders are compiled by a target of their own (`shading_contract_029_shaders`, inside the option), not added to `_contract_shader_names`;
- T-G1 does not require the implicit scene's own batch (F7).

## T043 approval record (maintainer, 2026-11-02)

The maintainer accepted the written tests and every recommendation of the Phase 2 report.

- **D1** accepted: `CShadeBatchReport { surface, globals, dim, displaceOnly, inShadow, rayDepth, illuminationHook, branch }` with `EShadeBranch` and `CShadingContext::shadeBatchObserver`. **D2** accepted: the read-only `tracedRays()` getter. **D3** accepted: `CRayLevel` is nested in `CShadingContext` and written `CShadingContext::CRayLevel` everywhere. **D4** accepted: `ShadingContext_Standalone` uses only what Phase 5 adds (no `tracer()`). **D5** accepted: `ShadingContract_PhotonLookup` relies on `CShadingState::services` staying public. D1 to D5 are written into the contracts (`shading-context.md`, `hider.md`, `point-source-and-globals.md`, `photon-lookup.md`, `link-and-tests.md`) as amendments of the T043 approval.
- **D6** accepted: `ShadingContract_ProducerSituations` does not require the implicit scene's own 0-D shadow batch (tasks.md T038). **F7** (the implicit fixture draws nothing under the raytrace hider) is followed up by the coordinator; nothing more is done here.
- **D7** accepted: `tools/situations-parse-fix.patch` is part of T-G3 (tasks.md T039, T094, T095). **F6 is an in-spec fix of this spec's own edit and is not filed.** Consequence for Phase 7, written into T039 and T095: against today's `classifyShading()` the test is red on 18 assertions, row 6's classify-only shadow case and 17 REYES reflection hits that come out `None` instead of `Secondary`; every one must turn green through the corrected `classifyShading()` (P-Q5) or be explained here before Phase 7 can pass; no expectation is relaxed.
- **Departures accepted:** the new CMake block is at the end of `tests/shading_contract/CMakeLists.txt`; `SHADING_CONTRACT_IMPLICIT_DIR` is in the shared `SHADING_CONTRACT_ENV`; the six new shaders have a target of their own (`shading_contract_029_shaders`); `Link_ShadingSelfContained` uses `dyld_info -imports` (F8); the Q5 edit is a patch applied in Phase 7.

## Phase 3 (US5 and US2: the bundles and the photon map; 13 allowlist lines)

T044 approved by the maintainer 2026-11-02 (with the moved line numbers and the 13 deletions by key). F7 is filed as GitHub #144 (see §4).

### §2 Test gate (T045), macOS

- Enabled: `Link_ShadingSelfContained` (T-L1), `ShadingContract_PhotonLookup{Interp,Jit}` (T-P1), `ShadingContract_CannedBundles{Interp,Jit}` (T-B1), `ShadingScope_Greps` with `OR029_SCOPE_CHECKS = S1a;S1b`. The two programs and `cm_photon` joined the normal build (`or029_contract_program` and the shader loop moved out of the option block; `add_contract_test_029` gained `ENABLED`).
- The 13 allowlist lines deleted by key (8 DB-005, 5 DB-001); no other line touched.
- Red, before the code: `test_shading_contract_photon_lookup` fails to build (`libshader/shading/photonLookup.h` not found), `test_shading_contract_canned_bundles` too (`libshader/shading/bundles.h` not found); `Link_ShadingSelfContained` fails on the 4 imports; `ShadingScope_Greps` fails S1a (8 lines) and S1b (6 lines); `ctest -L architecture` shows **exactly the 13 `VIOLATION` lines** (8 include, 5 link, `[Shading -> Hiders]`) and nothing else.
- Mutation check of S1a/S1b (scratch, reverted, `git diff` empty): adding `#include "hiders/photonMap.h"` and `#include "irradiance.h"` to `situation.cpp` adds exactly 4 failure lines naming that file (2 per sub-check); with it reverted there are none.

### Implementation (T046 to T052)

- `src/ri/hiders/bundles.{h,cpp}` moved with `git mv` to `src/libshader/shading/` (byte for byte); `bundles.cpp` joined `shading_sources`, left `ri_sources`. `ribVector` (which compiles `ri_sources`) lost `bundles.cpp` with it and still builds.
- Vestigial includes deleted (the build decided): `irradiance.h` and `photonMap.h` from `execute.cpp` and `shading.cpp`; `bundles.h` from the five outside includers (`renderer.cpp`, `rendererContext.cpp`, `rendererStatics.cpp`, `irradiance.cpp`, `irradianceDispatch.cpp`): none uses a bundle type. The remaining `bundles.h` includers are Shading's four files.
- `src/libshader/shading/photonLookup.h` (`IPhotonLookup`, inline defaulted virtual destructor, one pure virtual `lookup(float *, const float *, int)`); `CPhotonMap` derives from it (`lookup(float *, const float *, int) override`; `balance`, `draw`, `bound` and `typeName` also gained `override`, which clang's `-Winconsistent-missing-override` requires once one member says it). `getPhotonMap()` returns `IPhotonLookup *` in `rendererServices.h`, `rendererGetPhotonMap` (`shading.h`/`shading.cpp`), `jitPhotonMap`, `giFunctions.h` (both macros), `CPhotonMapLookup::map` (`shaderPl.h`) and `CRendererServicesImpl` (the same object, upcast); the two `class CPhotonMap;` forward declarations in Shading are gone. The six doubles' `getPhotonMap` signature (compile-fix, T044) is the only test-side edit.
- `kRendererServicesVersion` is **2.1.0**; the rule comment and the 2.1.0 history entry carry Phase 3's two lines (`getPhotonMap` changed, `IPhotonLookup` added). No `static_assert` line is edited (CP-v: empty). Comments naming the load stub or the imports corrected in `rendererServices.h` and `llvmJit.cpp`.
- Include path (T051): `src/ri/hiders` is off `libshader_shading`'s include path, and `render`, `output` and `dataviewer` went with it (all four removed at once; the compile needed none of them). `geometry`, `state`, `parse`, `texture`, `core` and `src/ri` stay (`geometry` until Phase 7). DB-023 note: the path lost 4 of 10 directories.
- `tests/shading_contract/load_stub.cpp` deleted (`git rm`); the source list and the comments that cited it edited as T044 lists (`CMakeLists.txt:139,155`, `case_link.cpp`, `case_no_tracer.cpp`, `case_gather_requests.cpp`, `recording_tracer.h`, `case_trace_requests.cpp`); `ShadingContract_StandaloneLink` and `ShadingContract_StandaloneLinkLine` pass without it.

### §1 Phase 3 checkpoint, macOS (CP-a to CP-d, CP-v)

- CP-a: build exit 0 under `-Werror`, no warning, every target (`ribVector` included).
- Imports (T053): `nm -m libshader_shading.1.0.0.dylib | grep -c 'dynamically looked up'` = **0** (was 4); `dyld_info -imports` shows no `<flat-namespace>` entry (`tools/libshader-imports-after-phase3-macos.txt`). `Link_ShadingSelfContained` and `ShadingScope_Greps` (S1a, S1b) **pass**.
- CP-b: 1058 tests (1052 + the 6 enabled), **1054 pass, 4 fail**: `ShadingContract_PhotonLookupInterp`, `..Jit`, `ShadingContract_CannedBundlesInterp`, `..Jit` (see "The four new tests" below); every other test is green, the replay and `Visual_*` included.
- CP-d: 900 images, **0 differing, 0 missing**; every `MaxBlockAvgDiff` equal.
- CP-c: `ctest -L architecture` 21 of 21; `ALLOWED DB-004 x 52, DB-006 x 31, DB-010 x 8, DB-011 x 166, DB-015 x 10, DB-017 x 78, DB-025 x 12`: **DB-001 and DB-005 have no edge left**; `RESULT PASS`; `METRIC` unchanged (TOTAL 612, Shading 0).
- CP-v: no `static_assert` edited.
- T054 (informational): `Link_StaticConsumer` **already passes** with `-undefined dynamic_lookup` still in the link line (both formats), as expected. P-Q17: relinked with `-Wl,-why_load`, `ribOut.cpp.o` and `rendererContext.cpp.o` are **still pulled** (95 members, 96 before), by the same references; the image is written and `RiBegin` reports no `CODE_INCAPABLE` (`tools/red-backend-after-phase3-macos.txt`).

### The four new tests that are red (a TEST-APPROVAL; none of this is an implementation problem)

With the implementation in, `ShadingContract_PhotonLookup{Interp,Jit}` and `ShadingContract_CannedBundles{Interp,Jit}` run and fail for reasons in their own assumptions, found now because Phase 2 could only compile them against stubs. Each was diagnosed and a fix applied transiently (evidence only; reverted, `git diff` empty); with the fixes all four **pass**. `tools/phase3-test-fixes.diff` is the proposed edit (not applied):

1. **T-P1, the derivative tail layout (both backends).** `expandVector()` fills the tail block-contiguously: `[real points, the du-offset copies, the dv-offset copies]` (measured: the tail's values are real 0..3, 0..3 in order), and the JIT's `jitPhotonMap` says the same ("block-contiguous [real, +du, +dv]"). The case asserted the interleaved layout (`tail[2i]`, `tail[2i+1]` = real `i`). Fix: `ci(numReal + j) == ci(j % numReal)` for `j < 2 * numReal`.
2. **T-P1, the estimator (JIT).** The interpreter honours `photonmap(..., "estimator", 50)`; the JIT's `jitPhotonMap` reads the estimator from the object's attributes only (none in the standalone program: 0), ignoring the parameter list. Fix: expect 50 under the interpreter and 0 under the JIT. This is a pre-existing interpreter/JIT difference, out of 029's scope (finding F11).
3. **T-B1, gather() has no object.** `CShadingContext::gatherSample()` reads `currentShadingState->currentObject->attributes` unconditionally (and `callAmbient`/`prepareAmbient` do too), so a gather() in the standalone program, which has no `CSurface`, segfaults (crash report: `gatherSample` called from `execute`). This is outside FR-027's scope (finding F12). Fix: the gather half of T-B1 comes out of Phase 3 and returns in Phase 7, when `setShadedObject()` lets the standalone program supply attributes without a `CSurface`; trace() and transmission() stay (they pass). F12.

Phase 3's CHECKPOINT (CP-b all green, T053, T055, T056) waits for this approval.

### §3 Phase 3 on Linux (GCC 13.3, `-Werror`, `-j2`, remote `cpujob`; CP-L)

- `linux_sync.sh push`: 3,559 files, 0 differences (it removed the stale `load_stub.cpp` from the remote); configure and build exit 0, **0 warnings** under `-Werror` (`bundles.cpp` in `libshader_shading`, `photonMap.h` overrides, the shorter include path all accepted by GCC).
- Imports: `nm -D --undefined-only libshader_shading.so` has **461** undefined symbols (465 before), none of the four, and **none defined by `libRIBRender.so`** (`tools/libshader-imports-after-phase3-linux.txt`).
- CP-b: 1059 tests, **10 fail**: the baseline's failing set **minus `LibShader_McompConsistency`** (#44: its cause, the `CTransmissionBundle` import, is gone, so this spec turns it green as SC-009 expects) = `CoverageAnalytic_alpha-sides2-tjunction-r4_reyes` (#57), `TextureTile_RenderByteIdentical` (#45), the four `TextureTile_Cache*` (#46), plus the four new tests of the TEST-APPROVAL above (`PhotonLookup{Interp,Jit}` fail, `CannedBundles{Interp,Jit}` segfault, as on macOS).
- CP-d: 900 images, **0 differing, 0 missing**; the `MaxBlockAvgDiff` listing equals the baseline's.
- CP-c: 21 of 21; `ALLOWED DB-004 x 52, DB-006 x 31, DB-010 x 8, DB-011 x 166, DB-015 x 10, DB-017 x 78, DB-025 x 12`; DB-001 and DB-005 have no edge; `RESULT PASS`.

### §4 Phase 3 findings

- **F11 (the JIT's `photonmap()` ignores the call's `"estimator"` parameter).** The interpreter's macro uses `scratch->photonmapParams.estimator` when set; `jitPhotonMap` reads `cAttributes->photonEstimator` only. A shader that passes `"estimator", N` gets N under `.rslo` and the attribute under `.slo`. Pre-existing, out of 029's scope (the texture calls were made consistent by spec 027; photonmap was not); T-P1's proposed fix asserts the difference as it is. A documented spec 017 scoping (`shading.h:873-878`); filed as **GitHub #145** (P1).
- **F12 (`gather()` reads the current object's attributes with no null check).** `CShadingContext::gather()`'s sampling (`gatherSample()`, `shading.cpp:4286`) and `callAmbient()` (`:1833`), `prepareAmbient()` (`:2062`) and the light loop (`:1774`) dereference `currentShadingState->currentObject->attributes` unconditionally, so a standalone program (no `CSurface`) segfaults in `gather()`. Pre-existing and **outside FR-027's scope** (spec 026's FR-027 covers only the six object builtins and `shadername(type)`; this is not a contradiction of it). Filed as **GitHub #146** (P1): `ambient()` in an imager shader segfaults because the GI builtins dereference a null shaded object; confirmed in a real render (the imager runs with `currentObject = nullptr`, `imager.cpp`, and `callAmbient()` crashes; an imager `gather()` did not crash in that scene). Not fixed here; after Phase 7's `setShadedObject(nullptr, handle, attributes)` a standalone program can give gather its attributes, which is where T-B1's gather half returns.

### T053 to T057 after the approval of `tools/phase3-test-fixes.diff` (maintainer, 2026-11-03)

The diff was applied as written (`case_photon_lookup.cpp`, `case_canned_bundles.cpp`). F11 is GitHub #145 and F12 is GitHub #146 (both P1); F12 is outside FR-027's scope; neither is fixed here.

### Phase 3 CHECKPOINT 3 (after the approved test fixes)

| | macOS | Linux (GCC 13, `-Werror`, `-j2`) |
|---|---|---|
| CP-a build | exit 0, 0 warnings | exit 0, 0 warnings |
| CP-b suite | **1058 of 1058 pass** (the 6 newly enabled included), 1 self-skip as in the baseline | 1059 tests, 6 fail: **the baseline's set minus `LibShader_McompConsistency` (#44, turned green by this spec)**: `CoverageAnalytic_alpha-sides2-tjunction-r4_reyes` #57, `TextureTile_RenderByteIdentical` #45, `TextureTile_Cache{ByteIdentical,DefaultDirectory,StaleDetection,ConcurrentWrite}` #46 |
| CP-d images | 900, **0 differing, 0 missing**; `MaxBlockAvgDiff` listing equal | 900, **0 differing, 0 missing**; listing equal |
| CP-c architecture | 21 of 21; `ALLOWED DB-004 x 52, DB-006 x 31, DB-010 x 8, DB-011 x 166, DB-015 x 10, DB-017 x 78, DB-025 x 12` (DB-001 and DB-005 gone); `RESULT PASS`; TOTAL refs 612 (baseline 612) | 21 of 21; same `ALLOWED`; `RESULT PASS` |
| CP-m (`-L '^shading_contract$\|^link$'`) | 61 of 61 pass, 11 Not Run (Disabled); replay green; `tests/shading_contract/replay` and `tools/baseline*` unchanged | all pass, 11 Not Run (Disabled) |
| CP-v | no `static_assert` line edited | same tree |

Imports: macOS 0 dynamically looked up (was 4); Linux 461 undefined symbols (465 before), none of the four, none defined by `libRIBRender`. Findings of the phase: F11 is GitHub #145 and F12 is GitHub #146 (both P1, out of spec); the remaining Phase 3 tests are green.
