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

Phase 1's red evidence is the two red links and the two package-consumer reds recorded above; the test reds begin in Phase 2.

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
