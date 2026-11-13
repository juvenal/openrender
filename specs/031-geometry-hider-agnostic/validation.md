# Validation record: spec 031, Geometry, Hider-Agnostic

Worktree `/Volumes/Projects/Development/CLI/openrender-worktrees/031-geometry-hider-agnostic`, branch `031-geometry-hider-agnostic`,
base master `e83d005d`. macOS: Apple Silicon, Clang, Release, JIT on, `-Werror`, LLVM 23.1.1 core-only (vcpkg). Every job through `cpujob`.

## §0 Starting-tree measurements and findings

### Orientation (T001)

- `git log -1` is `99d382fe` ("docs: add spec 031 tasks and tasks-stage amendments"), on top of `9491e766` and base `e83d005d`. **tasks.md and the
  brief expected HEAD `9491e766` with the tasks-stage and analyze-stage edits staged, not committed. They are committed in `99d382fe`**, and
  `git status` was clean at the start (nothing staged, nothing unstaged). Nothing to fix: the documents Phase 1 needs are all in HEAD. Recorded, not repaired.
- `CLAUDE.md`: the worktree copy and the main checkout's copy are byte-identical (`diff`), both with the "Spec-kit agent rules" section; followed.
- `.specify/extensions.yml`: every `speckit.git.commit` hook (`before_*`, `after_*`) is `optional: true`; declined.
- `df -h /Volumes/Projects` at the start: 6.3 GB free (79%); at T005 configure 5.6 GB; after the build 4.5 GB; later 2.5 GB (other specs build in parallel).

### Tools (T002 to T004, T011)

- `tools/image_manifest.sh` and `tools/timing_sc010.sh` copied from spec 029 with `cp -p` (`image_manifest.sh` is byte-identical: same sha256).
- `tools/linux_sync.sh` is 029's script with every 029 reference changed (remote root `Projects/Development/CLI/openrender-031`, the `*/openrender-031`
  case, the spec-directory existence check, the Linux-only glob and `is_linux_only`, the `mkdir -p` of `pull`). Not yet run (see §3).
- `tools/timing_sc010.sh` (the copy only; `bash -n` clean): scenes `killeroo-reyes` (heavy REYES grid), `csg-hero-displaced-raytrace` (displaced
  raytrace, `bump_lowfreq`, 800x500, 4x4 samples), `blobby-csg-difference-reyes` (CSG), `camera-dof-reyes`, `camera-dof-raytrace` (one depth-of-field
  scene per hider). `SCALES` were confirmed by the T012 runs (see "T011/T012" below).

### macOS baseline build and suite (T005 to T007)

| Step | Result |
|---|---|
| T005 configure (`031-base-configure`) | clean; `-DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` (**the compile-commands option is an addition of this agent**, needed by the T013 probe; it changes no build output) |
| T005 build (`031-base-build`, `-j4`) | exit 0; **0 compiler warnings** (24 lines match "warning": `ld` duplicate-library and macOS-13.0 notices, one `run_actool.cmake` CMake warning); `build` is 580 MB on disk |
| T006 suite (`031-base-suite`, `-LE determinism-sweep -j4`) | **1097 tests, 100% passed** (293 s); 1 test skips itself: `LibShader_Format_rslo_lower_minor` (so 1096 ran and passed). No known-flaky test failed. 915 images in `tools/baseline-images-macos.sha256`; 389 rows in `tools/baseline-diffs-macos.txt` |
| T007 noise (`031-base-suite2`) | second run: 1097 tests, 100% passed (239 s); **915 images, 0 differing**; the `MaxBlockAvgDiff` listing is identical (diff empty). `tools/baseline-image-noise-macos.txt` holds a comment only |

### T008 architecture report and library facts (macOS)

`tools/archcheck-base-macos.txt`:

```
ALLOWED   DB-004 x 52 DB-006 x 30 DB-010 x 8 DB-011 x 164 DB-017 x 78 DB-025 x 12     (344 entries; the 8 of other ids are DB-010)
METRIC    Geometry refs 48 (baseline 48)  distinct 15 (baseline 15)
METRIC    Hiders refs 556 (baseline 556)  distinct 81 (baseline 81)
METRIC    Infra / SceneInterface / Shading / Texturing refs 0
METRIC    TOTAL refs 604 (baseline 604)  distinct 89 (baseline 89)
RESULT    PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0
```

Equal to the expected values. `tools/ribvector-undefined-base-macos.txt`: `nm -u` over the 67 objects under `build/src/ri/CMakeFiles/ribVector_obj.dir`
(**found with `find ... -name '*.o'`, not the plan's one-level glob; 67 objects = the 67 `ribVector_obj` entries of `compile_commands.json`**), demangled
with `c++filt`, sorted unique: 773 lines. It names `CShadingContext`, `CReyes` (e.g. `CSurface::dice(CReyes*)`, `CDelayedInstance::intersect(CShadingContext*, CRay*)`)
as the plan expects, so T029's check has something to find on the starting tree.

### T009 macOS sweep (after the maintainer freed disk: 8.6 GB free)

A first attempt was stopped by the disk stop condition (97 of 419 passed, then `/Volumes/Projects` fell to 1.5 GB free because of other worktrees' builds;
this tree stayed at 580 MB). Rerun after the maintainer freed disk, with a watchdog at 2.2 GB free (never triggered):

| Run | Result |
|---|---|
| `031-base-sweep1` (`-L '^determinism-sweep$' -j2`) | **419 of 419 passed** (611 s); 419 images in `tools/baseline-sweep-images-macos.sha256`; log `tools/base-sweep-LastTest-macos.log` |
| `031-base-sweep2` (noise) | **419 of 419 passed** (574 s); **0 images differ** from run 1; `tools/baseline-sweep-noise-macos.txt` holds a comment only |

### T010 build-base (macOS)

`cp -Rp build build-base` (580 MB, `031-build-base` job) then a rewrite of every Mach-O in the copy: 217 files touched (`LC_RPATH` entries `build/...` to
`build-base/...`, `-change` of any `build/` install name, `-id` of dylibs, `codesign -f -s -`), by a python wrapper over `otool`/`install_name_tool`.
Verified: 1105 Mach-O files in `build-base`, **0 still name `$W/build/`**; `codesign -v` passes on `orender`, `oshader`, `libRIBRender.1.dylib`, `libRIBCommon.1.dylib`;
`otool -L build-base/src/orender/orender` names no `build/` path and its two RPATHs are `build-base/src/ri` and `build-base/src/common`. Dynamic check (`031-base-frozen`,
`DYLD_PRINT_LIBRARIES=1` over `camera-dof-reyes.rib` with the environment of `build-base`; exit 0): every openRender image loaded is under `build-base/`
(`orender`, `libRIBRender.1.0.0.dylib`, `libRIBCommon.1.0.0.dylib`, `libRIBShadeop.1.0.0.dylib`, `libshader_shading.1.0.0.dylib`, `file.dsply`); none from `build/`.

### T011/T012 timing baseline (macOS, `build-base` against `build-base`: the spread of each scene)

`$CJ 031-sc-base3 timing_sc010.sh build-base build-base 5` (5 interleaved series; `tools/timing-base-macos.txt`). Confirmed `SCALES`
(the first guesses left three scenes under the 3 s floor, flagged `TOO SHORT`, and were raised twice; the final values are in the copy):
`killeroo-reyes:format=3,shadingrate=4 csg-hero-displaced-raytrace:format=2.5 blobby-csg-difference-reyes:format=5,shadingrate=4 camera-dof-reyes:format=2,shadingrate=2 camera-dof-raytrace:format=4`.

| Scene | median "start" (s) | spread | median "final" (s) | spread |
|---|---|---|---|---|
| `killeroo-reyes` | 3.58 | 0.46 | 3.58 | 0.71 |
| `csg-hero-displaced-raytrace` | 9.48 | 2.71 | 9.48 | 1.31 |
| `blobby-csg-difference-reyes` | 3.78 | 0.49 | 3.85 | 0.76 |
| `camera-dof-reyes` | 4.00 | 0.56 | 4.01 | 0.57 |
| `camera-dof-raytrace` | 4.62 | 0.44 | 4.65 | 0.49 |

All medians over 3 s; every difference of medians within spread (the two trees are the same binary, so this is the noise floor: up to 2.7 s on the displaced raytrace scene).

### T013 include-fallout probe

See `tools/include-fallout.txt`. 627 TUs of `compile_commands.json`; the control pass over the unmodified scratch copy: 0 failing. After stripping the six
core includes from the five Geometry headers that have them, **exactly one file outside Geometry owes an error to the stripped includes:
`src/ri/hiders/photonMap.cpp`** (4 errors, first at `:102`, `CRenderer` undeclared). No other Hiders, Texturing, Graphics State or Shading file, no Core file
and no test file. (A first pass without `-ferror-limit=0` reported 57 non-Geometry failures, all stopped by Geometry's own `CRenderer` use in `object.h:97`;
that is not fallout and is recorded in the tools file so nobody trusts the 57.) `build-scratch` (42 MB, sources only) was deleted after the maintainer's approval (see T020 outcome).

### T014 test-edit inventory

`tools/test-edit-inventory.txt` (measured, not inherited). The plan's floor entries are all present (`csgTreeHelpers.h:66,72,98`, `test_camera_generate_ray_only.cpp:105`).
The non-Geometry subclasses found: `CPhonySurface` (`hiders/photon.cpp:51-56`, `instantiate`) and `CCaptureHolder` (`render/rendererContext.cpp:498-502`,
`instantiate` and `intersect`), plus the test doubles `rendered_harness.h:41`, `test_ambient_accumulation.cpp:71`, `test_false_if_constructs.cpp:78`,
`texture_substitute_harness.h:59` (instantiate only) and the three CSG test doubles. The four candidate files of Gap 5 (`test_sampling_option_rsl.cpp`,
`test_context_no_services.cpp`, `test_derivative_inactive_grid_lanes.cpp`, `test_derivative_inactive_lanes.cpp`) use `CShadingContext` as a Shading type only: **no edit, re-confirmed**.
Nothing overrides `dice(CReyes *)` / `splitToChildren(CReyes *)` outside Geometry (Phase 5 has no test override to edit).

### T015 findings F1 to F5 (verified against the tree)

- **F1** confirmed: `hiders/raytracer.cpp:391` and `hiders/photon.cpp:74` do `CRenderer::raytracingFlags |= ATTRIBUTES_FLAGS_PRIMARY_VISIBLE` in their constructors; `beginFrameParams(samplingConfig)` is at `render/renderer.cpp:805`, the context loop at `:812-832`. A frame copied at `:805` would miss the bit. T038 installs after the loop (Q2, answered yes).
- **F2** confirmed: the DB-017 link keys are mangled signatures (`allowlist.txt:277,279,292,296`). Counted from the allowlist, symbol by symbol: `CSurface::regionCoverage` 11, `CSurface::shade` 9, `CSurface::intersect` 6, `CSurface::checkRayGuard` 3, `CObject::cluster` 1, `CDelayedObject::intersect` 1, `CDelayedInstance::intersect` 1 = **32 for Phase 4**; `CSurface::dice` 7, `CObject::dice` 6, `CCSGSolid::dice`, `CPoints::dice`, `CCurve::dice`, `CDelayedObject::dice`, `CDelayedInstance::dice` (5), `CLinearCurve::splitToChildren`, `CCubicCurve::splitToChildren` (2) = **20 for Phase 5**; `typeinfo for CSurface` 14 + `vtable for CSurface` 11 = 25 unchanged; `CRenderer::findCoordinateSystem` 1 (Phase 3 deletes it). Sum 78.
- **F3** confirmed: `render/geometryDispatch.cpp:284,294,337,348` take `CRenderer::tesselateMutex` in `CSurface::intersect`/`checkRayGuard`.
- **F4** confirmed with one drift: core callers of `CRenderer::findCoordinateSystem` are `render/rendererContext.cpp:3307,3319,3321`, `render/init.cpp:98` and `tests/shading_contract/case_real_answers.cpp:54`; **the task text also names `rendererShadingServices.cpp:95`, which no longer calls it** (it answers through `getMatrix` since spec 026). The static stays; only Geometry's caller moves.
- **F5** confirmed: `reyes.cpp` calls the forwarding entry `context.displace(...)` at `:789,:869,:935,:1046,:1103` and `context.shade(...)` at `:906,:967,:1060,:1114` (the other `shade` call sites in the tests are the `ShaderGlobals` overload and do not change).

## §1 Checkpoint records

| Phase | CP-a | CP-b | CP-c | CP-d | CP-r | CP-det | CP-m | CP-Lb |
|---|---|---|---|---|---|---|---|---|
| 3 | macOS Release build exit 0, 0 compiler warnings (first build stopped only on the two compile-only test fixes of §2); Linux incremental build exit 0, **0 warnings** under GCC 13 `-Werror` | macOS **1099/1099** (the baseline's 1097 plus `Geometry_ObjectSink`, `Geometry_FrameDefault`); re-run on the final tree (after `clang-format` of 3 files) 1099/1099 | `ctest -L architecture` 29/29 on both hosts; `ALLOWED DB-004 x 33, DB-006 x 30, DB-010 x 8, DB-011 x 164, DB-017 x 77, DB-025 x 12` = **324**; METRIC Geometry refs 48 -> **30** (distinct 15 -> 9), Hiders 556/81 unchanged, TOTAL 604 -> 586 (the metrics baseline file is not lowered until T077); `RESULT PASS` | **0 differing, 0 missing** of 915 images (`image_manifest.sh diff` with the macOS baseline), `MaxBlockAvgDiff` listing identical (empty `diff`), on both the first and the final tree | `Motion_(Replay|CameraSlerpEquivalence)` 10/10 on both hosts; `git diff --stat -- tests/unit/motion/replay` empty | `^determinism$` 156/156 (macOS) | `-L '^geometry_contract$'` 2/2 (`Geometry_ObjectSink`, `Geometry_FrameDefault`) on both hosts; later tests not built | Linux push 3,754 files, `verify` 0 differences; build, `-L architecture` 29/29, contract 2/2, motion 10/10 |
| 2 | macOS: red-evidence configure ON then OFF, 0 `Building` lines after OFF (nothing rebuilt); Linux: build exit 0, 0 warnings | not re-run (nothing but gated tests and CMake changed; the baseline suite is the Phase 1 record) | `ctest -L architecture`: 29 of 29 on both hosts; report: 344 entries (DB-004 52, DB-006 30, DB-010 8, DB-011 164, DB-017 78, DB-025 12), Geometry 48/15, Hiders 556/81, TOTAL 604/89, `RESULT PASS` | n/a (no image-producing code changed) | n/a | n/a | `-L '^geometry_contract$'` finds no test with the option OFF (both hosts) | done, `Motion_*` 10/10 |
| 1 | macOS build exit 0, 0 warnings; Linux build exit 0, 0 warnings | macOS 1097/1097 (1 self-skip), twice, sweep 419/419 twice; Linux 1097/1098 (#45) twice, sweep 419/419 twice | `ctest -L architecture`: 29 of 29 on both hosts (344 entries, RESULT PASS) | baselines recorded (0 noise on both hosts) | n/a | n/a | n/a | baseline built and verified (§3) |

### Phase 4 (macOS; the Linux half is recorded in §3)

| CP | Result |
|---|---|
| CP-a build (`031-p4-build`, `031-p4-build2` after the `clang-format` of 3 files) | exit 0, 0 first-party compiler warnings under `-Werror` (the only "warning" lines are `ld` duplicate-library notices); every target including `ribVector` and the data viewer built. One first-pass error, an unused parameter in `CLoopSubdivMesh::create` (it used to pass the context to `setChildren`), fixed by dropping the name |
| CP-b suite (`031-p4-suite`, `-LE determinism-sweep -j4`) | **1099/1099 passed** (the 1 self-skip is `LibShader_Format_rslo_lower_minor`), the Phase 3 count; no new test enabled in this phase |
| CP-d | `image_manifest.sh diff`: 915 images, **0 differing, 0 missing, 0 not in manifest**; `diffs` against `baseline-diffs-macos.txt`: empty (every `MaxBlockAvgDiff` equal) |
| CP-r | `Motion_Replay_*` and `Motion_CameraSlerpEquivalence`: 10/10; `git diff --stat -- tests/unit/motion/replay` empty |
| CP-det | `^determinism$` 156/156 (`-j2`) |
| CP-m | `^geometry_contract$` 2/2 (`Geometry_ObjectSink`, `Geometry_FrameDefault`); the later tests are not built |
| preview / viewer | the `SceneInterface_`/`Preview_` tests with label `preview`: 3/3; the data viewer is a build target and built |
| CP-c | `ctest -L architecture` 29/29. Report (`build-logs/p4-archreport2.txt`): `ALLOWED DB-004 x 31, DB-006 x 26, DB-010 x 8, DB-011 x 164, DB-017 x 77, DB-025 x 11` = **317** (the expected row of tasks.md); METRIC Geometry refs 48 -> **21** (distinct 15 -> 7), Hiders 556/81 unchanged, TOTAL 604 -> 577 (distinct 89 -> 84); `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0` |
| T055 `tesselationOverhead` comparison | `csg-hero-displaced-raytrace.rib` with `Option "statistics" "endofframe" [3]` and `-x 3 -t:1`, `build-base` against `build`: **identical** (Peak memory 10,685,796; memory 0; cache hits 5,403,084; misses 7,897; Tess. Overhead 1,585,840 bytes both) |

**Test gate (T049).** The 7 lines were deleted by key and `ctest -L architecture` was run on the Phase 3 build tree, before any rebuild: `RESULT FAIL violations=6 stale=0` with exactly the six link
`VIOLATION` lines (`dlobject.cpp` and `implicitSurface.cpp -> CShadingContext::shade`, `surface.cpp -> CShadingContext::restoreState`, `saveState`, `CRenderer::numThreads`, `CRenderer::tesselateMutex`; `build-logs/p4-tg-arch.log`).
**The seventh, DB-025 `surface.cpp -> dataviewer/debug.h`, did not show**: I had already edited `surface.cpp` (the `#include "debug.h"` was gone) before running the gate, and archcheck reads the include
edge from the source text, not from the stale depfile. This is an order slip (the gate should have run before the first source edit); the line is a fact of HEAD (`git show HEAD:src/ri/geometry/surface.cpp` has `#include "debug.h"` at `:33`)
and the line is gone from the allowlist with the include. The final archcheck shows no `STALE` and no `VIOLATION`.

**DB-017 re-key (Q1, T060): 32 lines, in place.** archcheck reported exactly 32 `STALE` (old key) and 32 `VIOLATION` (new key) lines after the build; each old line was rewritten with the new mangled name and demangled comment
(file, function and count unchanged, no line added; the names equal those proposed at PAUSE 4a). allowlist diff `-32/+32`:

| File | Old key | New key |
|---|---|---|
| `geometry/bsplinePatchgrid.cpp` | `_ZN8CSurface14regionCoverageEP15CShadingContextiiPKfS3_Ph` | `_ZN8CSurface14regionCoverageEP11IRayContextiiPKfS3_Ph` |
| `geometry/bsplinePatchgrid.cpp` | `_ZN8CSurface5shadeEP15CShadingContextiPP4CRay` | `_ZN8CSurface5shadeER11IRayContextiPP4CRay` |
| `geometry/bsplinePatchgrid.cpp` | `_ZN8CSurface9intersectEP15CShadingContextP4CRay` | `_ZN8CSurface9intersectER11IRayContextP4CRay` |
| `geometry/curves.cpp` | `_ZN8CSurface14regionCoverageEP15CShadingContextiiPKfS3_Ph` | `_ZN8CSurface14regionCoverageEP11IRayContextiiPKfS3_Ph` |
| `geometry/curves.cpp` | `_ZN8CSurface5shadeEP15CShadingContextiPP4CRay` | `_ZN8CSurface5shadeER11IRayContextiPP4CRay` |
| `geometry/curves.cpp` | `_ZN8CSurface9intersectEP15CShadingContextP4CRay` | `_ZN8CSurface9intersectER11IRayContextP4CRay` |
| `geometry/delayed.cpp` | `_ZN14CDelayedObject9intersectEP15CShadingContextP4CRay` | `_ZN14CDelayedObject9intersectER11IRayContextP4CRay` |
| `geometry/delayed.cpp` | `_ZN16CDelayedInstance9intersectEP15CShadingContextP4CRay` | `_ZN16CDelayedInstance9intersectER11IRayContextP4CRay` |
| `geometry/dlobject.cpp` | `_ZN8CSurface14regionCoverageEP15CShadingContextiiPKfS3_Ph` | `_ZN8CSurface14regionCoverageEP11IRayContextiiPKfS3_Ph` |
| `geometry/implicitSurface.cpp` | `_ZN8CSurface14regionCoverageEP15CShadingContextiiPKfS3_Ph` | `_ZN8CSurface14regionCoverageEP11IRayContextiiPKfS3_Ph` |
| `geometry/patches.cpp` | `_ZN8CSurface13checkRayGuardEP4CRayP15CShadingContext` | `_ZN8CSurface13checkRayGuardEP4CRayR11IRayContext` |
| `geometry/patches.cpp` | `_ZN8CSurface14regionCoverageEP15CShadingContextiiPKfS3_Ph` | `_ZN8CSurface14regionCoverageEP11IRayContextiiPKfS3_Ph` |
| `geometry/patches.cpp` | `_ZN8CSurface5shadeEP15CShadingContextiPP4CRay` | `_ZN8CSurface5shadeER11IRayContextiPP4CRay` |
| `geometry/patches.cpp` | `_ZN8CSurface9intersectEP15CShadingContextP4CRay` | `_ZN8CSurface9intersectER11IRayContextP4CRay` |
| `geometry/patchgrid.cpp` | `_ZN8CSurface14regionCoverageEP15CShadingContextiiPKfS3_Ph` | `_ZN8CSurface14regionCoverageEP11IRayContextiiPKfS3_Ph` |
| `geometry/patchgrid.cpp` | `_ZN8CSurface5shadeEP15CShadingContextiPP4CRay` | `_ZN8CSurface5shadeER11IRayContextiPP4CRay` |
| `geometry/patchgrid.cpp` | `_ZN8CSurface9intersectEP15CShadingContextP4CRay` | `_ZN8CSurface9intersectER11IRayContextP4CRay` |
| `geometry/points.cpp` | `_ZN8CSurface14regionCoverageEP15CShadingContextiiPKfS3_Ph` | `_ZN8CSurface14regionCoverageEP11IRayContextiiPKfS3_Ph` |
| `geometry/points.cpp` | `_ZN8CSurface5shadeEP15CShadingContextiPP4CRay` | `_ZN8CSurface5shadeER11IRayContextiPP4CRay` |
| `geometry/polygons.cpp` | `_ZN8CSurface13checkRayGuardEP4CRayP15CShadingContext` | `_ZN8CSurface13checkRayGuardEP4CRayR11IRayContext` |
| `geometry/polygons.cpp` | `_ZN8CSurface14regionCoverageEP15CShadingContextiiPKfS3_Ph` | `_ZN8CSurface14regionCoverageEP11IRayContextiiPKfS3_Ph` |
| `geometry/polygons.cpp` | `_ZN8CSurface5shadeEP15CShadingContextiPP4CRay` | `_ZN8CSurface5shadeER11IRayContextiPP4CRay` |
| `geometry/quadrics.cpp` | `_ZN8CSurface13checkRayGuardEP4CRayP15CShadingContext` | `_ZN8CSurface13checkRayGuardEP4CRayR11IRayContext` |
| `geometry/quadrics.cpp` | `_ZN8CSurface14regionCoverageEP15CShadingContextiiPKfS3_Ph` | `_ZN8CSurface14regionCoverageEP11IRayContextiiPKfS3_Ph` |
| `geometry/quadrics.cpp` | `_ZN8CSurface5shadeEP15CShadingContextiPP4CRay` | `_ZN8CSurface5shadeER11IRayContextiPP4CRay` |
| `geometry/subdivision.cpp` | `_ZN8CSurface14regionCoverageEP15CShadingContextiiPKfS3_Ph` | `_ZN8CSurface14regionCoverageEP11IRayContextiiPKfS3_Ph` |
| `geometry/subdivision.cpp` | `_ZN8CSurface5shadeEP15CShadingContextiPP4CRay` | `_ZN8CSurface5shadeER11IRayContextiPP4CRay` |
| `geometry/subdivision.cpp` | `_ZN8CSurface9intersectEP15CShadingContextP4CRay` | `_ZN8CSurface9intersectER11IRayContextP4CRay` |
| `hiders/photon.cpp` | `_ZN8CSurface14regionCoverageEP15CShadingContextiiPKfS3_Ph` | `_ZN8CSurface14regionCoverageEP11IRayContextiiPKfS3_Ph` |
| `hiders/photon.cpp` | `_ZN8CSurface5shadeEP15CShadingContextiPP4CRay` | `_ZN8CSurface5shadeER11IRayContextiPP4CRay` |
| `hiders/photon.cpp` | `_ZN8CSurface9intersectEP15CShadingContextP4CRay` | `_ZN8CSurface9intersectER11IRayContextP4CRay` |
| `hiders/trace.cpp` | `_ZN7CObject7clusterEP15CShadingContext` | `_ZN7CObject7clusterER11IRayContext` |

**Allowlist, whole phase**: 344 -> 324 at Phase 3, now **317** (7 deleted + 32 re-keyed). Lines that left with `numThreads`: only `surface.cpp -> CRenderer::numThreads` (and `tesselateMutex`, `restoreState`,
`saveState`); no further `surface.cpp -> CRenderer::` line went stale (the dice side keeps `flags`, `maxGridSize`, `maxEyeSplits`, `imagePlane`, `projection`, `dPixeldx`, ... for Phase 5).

**Lock design.** `CTesselationPatch::lockTessellation()`/`unlockTessellation()` (static, declared in `surface.h`, defined in `surface.cpp` over a namespace-scope `static std::mutex`) replace `osLock/osUnlock(CRenderer::tesselateMutex)`
at `surface.cpp` (`tesselate`, `purgeTesselations`) and `geometryDispatch.cpp` (`CSurface::checkRayGuard`, `CSurface::intersect`), same scopes and order. `CRenderer::tesselateMutex` is deleted (`renderer.h`, `rendererMutexes.cpp` definition, create and
delete). Phase 6 (T080) makes the pair TU-local. `CTesselationPatch` has `numThreadsAtBuild` (from `geometryFrame().numThreads` in the constructor), used by the constructor arrays, the destructor, `tesselationOverhead` and
`purgeTesselations`' callers; `shutdownTesselations()` asserts (debug only) `tesselationList == NULL` and `stats.tesselationMemory` equal to its value at `initTesselations`.

**Production edits of the phase**: `rayContext.h` (new); `CHider : IRayContext` (`hider.h`; `shade`/`displace` forward to the context's existing Geometry-facing entry, `saveShadingState`/`restoreShadingState` to `saveState`/`restoreState`);
every ray-side Geometry signature of the signature map (`CObject::intersect/cluster/materialize/regionCoverage`, `CSurface::shade/countCrossings/checkRayGuard`, the region hooks, `CCSGSolid::resolve/build`, the CSG classifiers, `countFreeFormCrossings`, the six mesh `create`,
`CTesselationPatch`'s functions); `CObject::setChildren` lost its first parameter (callers `renderer.cpp`, `rendererContext.cpp` x2, and the Geometry bodies); `CCaptureHolder::intersect`; the bodies in `geometryDispatch.cpp` and the stubs
(`geometryDispatchStub.cpp`, `surfaceCrossingDispatchStub.cpp`) track the signatures; `trace.cpp` passes `*this` (`:210`, `:338`, `:350`) and `reyes.cpp:1042` passes `this`; the dice-side bodies pass `*rasterizer`/`*r` to `create`/`resolve`/`classifyBound`
as transitional arguments (Phase 5 turns them into `dicer.rayContext()`). DB-025: `#include "debug.h"`, the dead `DEBUG_HITS` block and its two `debugHit()` calls, and the dead `DEBUG_TESSELATIONS` block of `surface.cpp` are deleted.
No float expression changed text. `clang-format` was applied to `dlobject.h`, `implicitSurface.h` (clean before) and the new `rayContext.h`; the other edited files were not clang-format clean before and were left alone.

**Approved test edits applied (T057)**: exactly the list of PAUSE 4a (15 test files; `git diff --cached --stat -- tests`), no assertion, threshold or recording changed.

### Phase 5 (macOS; the Linux half is in §3)

**Test gate (T065), run before any source edit.** The 15 lines were deleted by key and `ctest -L architecture` was run on the unchanged Phase 4 build tree: `RESULT FAIL violations=15 stale=0`
(`build-logs/p5-tg-arch.log`) with exactly the 15 link `VIOLATION` lines of PAUSE 5a: `surface.cpp -> CShadingContext::shade(CSurface*, int, int, EShadingDim, unsigned int, int)`, `CReyes::drawObject`, `CReyes::drawGrid`, and
`CRenderer::` `imagePlane`, `inFrustrum`, `projection`, `maxGridSize`, `maxEyeSplits`, `cocFactorPixels`, `invFocaldistance`, `flags`, `dPixeldx`, `dPixeldy`, `pixelTop`, `pixelLeft` (`failing-cycles=3` with them, gone with the code).

**Source work.** `dicer.h` (new, `IDicer`: `rayContext`, `drawObject`, `drawGrid` with no default argument, `drawPoints`, `probe`, `inFrustum`, `frame`; not derived from `IRayContext`). `CReyes : CHider, IDicer`: `rayContext()` returns `*this`,
`drawObject/drawGrid/drawPoints` keep their bodies, `probe` calls `shadeSurface(..., 3)`, `inFrustum` is out of line in `reyes.cpp` (`CRenderer::inFrustrum`), and `drawObject`'s own frustum test calls `CReyes::inFrustum` so the
file keeps one reference (Hiders refs stay 556; the first build had 557). Forwarding entries retired: `CShadingContext::shade(CSurface *, ...)`, `displace`, `displaceEstimate`, `setProducer`/`producerObject`, the `IShadingProducer`
forward declaration, `shadingProducer.h` (`git rm`), its include and the "no shading producer" warning; `CHider` lost the `IShadingProducer` base, `setProducer(this)` and the override: `shadeSurface` is a protected member without the context
argument, body unchanged, and `CHider::shade/displace` call it with FALSE/TRUE. **F5 as planned**: the nine `reyes.cpp` sites (`context.displace` at `:789,:869,:935,:1046,:1103`, `context.shade` at `:906,:967,:1060,:1114` before the `inFrustum`
insertion) now call `shadeSurface(object, ..., TRUE/FALSE)` with the same arguments, and `object->dice(this)` (`:374`, `:570`) became `dice(*this)`; no site differed from the plan. Geometry: every `dice`/`splitToChildren`/`buildLayout`/`probe`/`layoutNode`
takes `IDicer &`; `r->shadingContext()` became `dicer.rayContext()`, `displaceEstimate` became `dicer.probe`, the `CRenderer::` reads of `CPatch::probe/buildLayout/layoutNode/dice` and `sampleTesselation` (`maxGridSize`, which the Phase 4 note missed:
`surface.cpp:1693`) read `dicer.frame()` / `context.frame()` (one `const CGeometryFrame &frame` in `probe`), `cull` and `minCocPixels` take the frame (`minCocPixels` keeps its expression, `absf((1 / z) - frame.invFocaldistance) * frame.cocFactorPixels`,
the text of the old `cocPixels` macro), `camera2pixels(frame, ...)` is the frame's copy, `CRenderer::inFrustrum` became `dicer.inFrustum`; the dead `#else` branch of the `#if 1` in `probe` (`r->displaceEstimate`, ~63 lines) is deleted. The bodies in
`geometryDispatch.cpp` (still core-side, so its own `CRenderer::flags/clipMin/maxGridSize` reads stay until Phase 6) and the stubs track the signatures. `kRendererServicesVersion` is 2.2.0 with the "Breaking within 2.x" history entry; no test pins the
minor (five `static_assert(...major == 2)` only). **No approved test or harness was edited** (T071 empty: the re-grep of PAUSE 5a found nothing to change, and the build confirmed it).

**DB-017 re-key, part 2 (Q1, T073): 20 lines, in place.** archcheck reported exactly 20 `STALE` and 20 `VIOLATION` lines after the build; every new name equals the one proposed at PAUSE 5a (the script asserted it). allowlist diff `-20/+20`:

| File | Old key | New key |
|---|---|---|
| `geometry/bsplinePatchgrid.cpp` | `_ZN8CSurface4diceEP6CReyes` | `_ZN8CSurface4diceER6IDicer` |
| `geometry/csgSolid.cpp` | `_ZN9CCSGSolid4diceEP6CReyes` | `_ZN9CCSGSolid4diceER6IDicer` |
| `geometry/curves.cpp` | `_ZN11CCubicCurve15splitToChildrenEP6CReyes` | `_ZN11CCubicCurve15splitToChildrenER6IDicer` |
| `geometry/curves.cpp` | `_ZN12CLinearCurve15splitToChildrenEP6CReyes` | `_ZN12CLinearCurve15splitToChildrenER6IDicer` |
| `geometry/curves.cpp` | `_ZN6CCurve4diceEP6CReyes` | `_ZN6CCurve4diceER6IDicer` |
| `geometry/curves.cpp` | `_ZN7CObject4diceEP6CReyes` | `_ZN7CObject4diceER6IDicer` |
| `geometry/delayed.cpp` | `_ZN14CDelayedObject4diceEP6CReyes` | `_ZN14CDelayedObject4diceER6IDicer` |
| `geometry/delayed.cpp` | `_ZN16CDelayedInstance4diceEP6CReyes` | `_ZN16CDelayedInstance4diceER6IDicer` |
| `geometry/object.cpp` | `_ZN7CObject4diceEP6CReyes` | `_ZN7CObject4diceER6IDicer` |
| `geometry/patches.cpp` | `_ZN7CObject4diceEP6CReyes` | `_ZN7CObject4diceER6IDicer` |
| `geometry/patches.cpp` | `_ZN8CSurface4diceEP6CReyes` | `_ZN8CSurface4diceER6IDicer` |
| `geometry/patchgrid.cpp` | `_ZN8CSurface4diceEP6CReyes` | `_ZN8CSurface4diceER6IDicer` |
| `geometry/points.cpp` | `_ZN7CPoints4diceEP6CReyes` | `_ZN7CPoints4diceER6IDicer` |
| `geometry/polygons.cpp` | `_ZN7CObject4diceEP6CReyes` | `_ZN7CObject4diceER6IDicer` |
| `geometry/polygons.cpp` | `_ZN8CSurface4diceEP6CReyes` | `_ZN8CSurface4diceER6IDicer` |
| `geometry/quadrics.cpp` | `_ZN8CSurface4diceEP6CReyes` | `_ZN8CSurface4diceER6IDicer` |
| `geometry/subdivision.cpp` | `_ZN8CSurface4diceEP6CReyes` | `_ZN8CSurface4diceER6IDicer` |
| `geometry/subdivisionCreator.cpp` | `_ZN7CObject4diceEP6CReyes` | `_ZN7CObject4diceER6IDicer` |
| `geometry/subdivisionLoop.cpp` | `_ZN7CObject4diceEP6CReyes` | `_ZN7CObject4diceER6IDicer` |
| `hiders/photon.cpp` | `_ZN8CSurface4diceEP6CReyes` | `_ZN8CSurface4diceER6IDicer` |

| CP (macOS) | Result |
|---|---|
| CP-a build | exit 0, 0 first-party warnings under `-Werror`, first pass (`031-p5-build`); every target incl. `ribVector`, the preview and the data viewer |
| CP-b suite (`031-p5-suite`, `-LE determinism-sweep -j4`) | **1099/1099 passed** (self-skip `LibShader_Format_rslo_lower_minor`); the Phase 4 count, no test enabled |
| CP-d | `image_manifest.sh diff`: 915 images, **0 differing, 0 missing, 0 not in manifest**; `diffs` against `baseline-diffs-macos.txt`: empty |
| CP-r | `Motion_*` 10/10; `tests/unit/motion/replay` untouched |
| CP-det | `^determinism$` 156/156 (`-j2`) |
| CP-m | `^geometry_contract$` 2/2 (the Phase 3 pair) |
| `-L shading_parity` / `-L state` / preview tests | 136/136, 7/7, 3/3; the data viewer is a build target and built |
| Timing, information only (`031-p5-time`, `timing_sc010.sh build-base build 3`) | start vs final medians (s): `killeroo-reyes` 3.62/3.64, `csg-hero-displaced-raytrace` 9.93/9.91, `blobby-csg-difference-reyes` 3.85/3.95, `camera-dof-reyes` 4.35/4.42, `camera-dof-raytrace` 4.94/4.82; every difference within spread, 0 scenes slower beyond it (`build-logs/p5-timing.log`) |
| CP-c | `ctest -L architecture` 29/29; `ALLOWED DB-004 x 19, DB-006 x 23, DB-010 x 8, DB-011 x 164, DB-017 x 77, DB-025 x 11` = **302** (the table's row); METRIC Geometry refs 21 -> **2** (distinct 7 -> 2: the two `blobbyRepeller.cpp` `CRenderer::` uses), Hiders 556/81 unchanged, TOTAL 577 -> 558 (distinct 84 -> 83); `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0` |

### Phase 6, work so far (macOS; interim record at the stop for two approvals)

T079 to T084, T086 and T086a are done. `photonMap.cpp` reads `openrender::frameParams().camera.toWorld/fromWorld` (`:102-103`, `:212`, `:217`). The `geometryDispatch.cpp` bodies moved verbatim (receivers only; `clipMin`, `maxGridSize`, `flags` read
`dicer.frame()`) to `object.cpp` (`CObject::dice`, `cluster`), `csgSolid.cpp` (`CCSGSolid::dice`), `surface.cpp` (`checkRayGuard`, `intersect`, `dice`, `shade`, `regionCoverage`), `curves.cpp` (`CCurve::dice`, both `splitToChildren`), `points.cpp`
(`CPoints::dice`) and `delayed.cpp` (the host helpers and the four delayed bodies); the tessellation lock pair left `surface.h` and is an anonymous-namespace pair in `surface.cpp` (`grep -rn lockTessellation src` outside `surface.cpp`: nothing).
`surfaceCrossingDispatch.{h,cpp}` became `surfaceCrossings.{h,cpp}` (`git mv`); `geometryDispatch.cpp`, `rendererDeclarationsDispatch.cpp`, `geometryDispatchStub.cpp`, `rendererDeclarationsDispatchStub.cpp` and `surfaceCrossingDispatchStub.cpp`
are deleted (`src/ri/vector/` holds `irradianceDispatchStub.cpp` only); `notifyGlobalVariable` and `findCoordinateSystem` moved into `rendererDeclarations.cpp`, which leaves `libRIBVector`; `src/ri/CMakeLists.txt` and its comment are updated.
The include sweep removed 39 core/Shading/Reyes include lines from Geometry (all but `blobbyRepeller.cpp`'s `renderer.h`) and reworded the 24 comment-only names. Greps: `grep -rnE 'CReyes|CShadingContext|IShadingProducer|reyes\.h|shading\.h|shader\.h' src/ri/geometry`
prints nothing; `CRenderer|CRendererContext|CRiInterface` appear only at `blobbyRepeller.cpp:138`. T086a: `-fsyntax-only` of `surface.cpp`, `surfaceCrossings.cpp`, `object.cpp`, `curves.cpp`, `points.cpp`, `delayed.cpp` (ribVector include flags) and
`csgSolid.cpp` (renderer flags): 0 errors. `metrics-baseline.txt` set from the measured values (Geometry 2/2, Hiders 552/79, TOTAL 554/81, dated comment).

Build (`031-p6-build`, `-k`): every target built and linked clean (0 first-party warnings) **except** `test_csg_count_crossings`, which includes the renamed `surfaceCrossingDispatch.h` (an approved test; see the PAUSE 6 report). After removing stale
`geometryDispatch`/`*Stub` objects from `build/` (including a stale nested `build/tests/plugins/checks/install_layouts` tree that archcheck also scans and that the `plugin_abi` tests recreate), `ctest -L architecture` is 30/31 and archcheck reports
`ALLOWED DB-004 x 3, DB-010 x 8, DB-011 x 162, DB-025 x 11` = **184**, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0`. The one red test is `Geometry_RibVectorNoStubs`, for the irradiance symbols of §4 item 9 only
(`nm -u` of `ribVector_obj` names `CShadingContext` only in `irradianceSampleAccept(float, float, CShadingContext*)` and `CIrradianceCache::sample(..., CShadingContext*)`).

### Phase 6 checkpoint, first full run (macOS and Linux) and the two failing draft tests

Approved edits applied: `test_count_crossings.cpp` (include rename) and the two-symbol irradiance exemption in `check_ribvector_no_stubs.cmake` (merge note: §4 item 10). `031-p6-build2` (macOS, `-j4`): exit 0, 0 first-party warnings, every target.

| CP (macOS) | Result |
|---|---|
| CP-b suite (`031-p6-suite`) | 1102 of 1104 passed; the two failures are `Geometry_DiceThroughDicer` (SEGFAULT) and `Geometry_IntersectThroughRayContext` (7 assertions), see below; nothing else failed |
| CP-d | 915 images, **0 differing, 0 missing**; `MaxBlockAvgDiff` listing equal to the baseline |
| CP-r | `Motion_*` 10/10; replay recordings untouched |
| CP-det | `^determinism$` 156/156 |
| CP-m | 3 of 5: `Geometry_ObjectSink`, `Geometry_FrameDefault`, `Geometry_ExpansionThroughHost` pass; the two above fail |
| other labels | `-R 'SceneInterface_|Preview_|Geometry_'` 19/21 (the same two), `-L preview` green, `-L plugin_abi` green, `-L state` 7/7, `-L shading_parity` 136/136, `Visual_photon-parity` green |
| CP-c | `ctest -L architecture` **31/31** (including `Geometry_NoHiderShadingNames` and `Geometry_RibVectorNoStubs`); allowlist 184 (`DB-004 x 3, DB-010 x 8, DB-011 x 162, DB-025 x 11`) |

| CP-Lb (Linux, GCC 13 `-Werror`, `-j2`) | Result |
|---|---|
| push / verify | 3752 files, 0 differences |
| build (`031-l6-build`) | exit 0, 0 warnings |
| `ctest -L architecture` | 31/31 after the stale `geometryDispatch`/`*Stub` objects were deleted from the remote build tree (archcheck scans every object in the tree; the first run reported them as `Hiders -> Geometry,Core` violations); archcheck report identical to macOS (184) |
| `-L '^geometry_contract$'` | 3 of 5, the same two failures |
| `Motion_*` / `-L shading_parity` / `-L state` / `-L plugin_abi` | 10/10, 136/136, 7/7, 71/71 |

**The two failing tests are defects of the draft test code, not of the Phase 6 product code** (diagnosed with throwaway copies outside the tree, run through `cpujob`; the tree is unchanged). Causes, each a wrong assumption of the draft:
1. `Geometry_DiceThroughDicer` segfault: the recording dicer's `drawObject` kept a raw pointer, but `CSurface::dice` detaches its patch after queueing it (a hider holds a reference to what it queues), so the next `dice` used a freed patch.
2. The fixtures built patch, NURBS and point data at depth 0: those primitives take **camera-space** vertices (`RiPatch`/`RiPoints` hand them over transformed), so the object sat on the eye plane and dicing recursed to "too many eye splits"; the sphere's `vmin`/`vmax` were in radians within +-1 (a sphere's v range is +-pi/2, so the pole hit the intersection test expects was outside it).
3. `Geometry_IntersectThroughRayContext`: the test ray had `flags = 0` (rejected by `checkRayGuard`); `I` is `dir * t`, not `dir`; a mesh and a tessellation patch hand intersection to their children (the hider's trace loop walks the hierarchy), so the test must too; the displacing double must sample the undisplaced surface in `displace()` as a hider with no shader would (the dicing double already does); and the tessellation tables the core initialises at every frame start (`CTesselationPatch::initTesselations`) must be initialised and shut down by the test.
With those edits the three programs pass (throwaway builds): the diff is `tools/phase6-contract-test-fixes.diff` (3 files, +37/-15; new tests, so shown for approval; not applied).

**PAUSE 6 approval (2026-11-10).** The maintainer approved the contract test fixes of `tools/phase6-contract-test-fixes.diff` exactly as written (`fixtures.h`, `test_dice_through_dicer.cpp`, `test_intersect_through_ray_context.cpp`); applied after this entry.

### Phase 6 checkpoint, final run (after the approved contract test fixes; 2026-11-10)

| CP (macOS) | Result |
|---|---|
| CP-a build (`031-p6b-build`, `-j4`) | exit 0, 0 first-party warnings |
| CP-b suite | **1104/1104 passed** (the Phase 5 count plus the three newly enabled contract tests and the two checks) |
| CP-d | 915 images, **0 differing, 0 missing**; `MaxBlockAvgDiff` listing equal to the baseline |
| CP-r | `Motion_*` 10/10 |
| CP-m | **5/5** (`Geometry_ObjectSink`, `FrameDefault`, `DiceThroughDicer`, `IntersectThroughRayContext`, `ExpansionThroughHost`) |
| `-R 'SceneInterface_|Preview_|Geometry_'` | 21/21 |
| CP-c | `ctest -L architecture` **31/31**; allowlist 184 (`DB-004 x 3, DB-010 x 8, DB-011 x 162, DB-025 x 11`), `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0` |
| (earlier run, unchanged code) | CP-det 156/156, `-L preview`, `-L plugin_abi` 71/71, `-L state` 7/7, `-L shading_parity` 136/136, `Visual_photon-parity` 4/4 |

Linux CP-Lb (`031-l7-*`, GCC 13 `-Werror`, `-j2`): push/verify 3753 files, 0 differences; build exit 0, 0 warnings; `ctest -L architecture` 31/31; `-L '^geometry_contract$'` **5/5**; `Motion_*` 10/10.

### Phase 7 checkpoint (macOS; Linux below once its lock frees)

Source: `FrameParams` gained `cocFactorScreen`, `cocFactorSamples` (copied in `beginFrameParams`, 0 in the default value); `reyes.cpp` (constructor, `drawObject`, `copyPoints`, `insertGrid`) takes `const FrameParams &fp = frameParams()` once per function and reads
`fp.aperture`, `fp.invFocaldistance`, `fp.cocFactorScreen`/`cocFactorSamples` (the `cocScreen` macro expanded in its own operand order) and `fp.camera.hasRotation/relRotQ/relTrans`; the four inlined `slerpq` + `qtoR` pairs of `reyes.cpp` and the ones of
`stochastic.cpp` (`rasterBegin`) and `stochasticQuad.h` (per grid: `[[maybe_unused]] fp`, under `STOCHASTIC_MOVING`) are `openrender::motion::cameraRotationAt`; the `identQ` constants are gone; no translation expression and no `atTime*` form changed;
`reyes.h`'s comment corrected. `rasterCameraMotionTransform` untouched.

| CP (macOS) | Result |
|---|---|
| CP-a build (`031-p7-build`, `-j4`) | exit 0, first pass, 0 first-party warnings |
| CP-b suite | **1104/1104 passed** |
| CP-d | 915 images, **0 differing, 0 missing**; every `MaxBlockAvgDiff` equal to the baseline (the camera-motion scenes included) |
| CP-r | `-R Motion_`: **13/13**, which includes `Motion_CameraSlerpEquivalence`, `Motion_Replay_*` (recordings not touched) and `Motion_FrameParamsEqualCore` with its two new checks |
| CP-det | `^determinism$` 156/156 |
| CP-c | `ctest -L architecture` 31/31; `ALLOWED DB-004 x 3, DB-010 x 8, DB-011 x 151, DB-025 x 11` = **173**; `RESULT PASS violations=0 stale=0` |
| Metric | Hiders 552 -> **499** references (79 -> 72 members), TOTAL 554 -> **501** (81 -> 74); Geometry 2/2 unchanged; `metrics-baseline.txt` lowered with a dated comment |

### Phase 8 interface-phase gate (T101-T109; 2026-11-11)

One source edit in the phase, comment-only: `dicer.h:36` named "CSG resolve"; FR-019's grep (`grep -nE 'CSG|Region'` over the four interface headers) must print nothing, so it now reads "lazy resolution and child creation" (macOS rebuilt with it before the run; Linux built before the push of that comment and was rebuilt after the runs, below).

| Check (tree `601df3f1` + the `dicer.h` comment) | macOS | Linux (GCC 13 `-Werror`, `-j2`) |
|---|---|---|
| CP-a build | exit 0, 0 first-party warnings | exit 0, 0 warnings (twice: before the runs, and after the `dicer.h` comment) |
| CP-b suite, fresh stamp (`-LE determinism-sweep`) | **1104/1104** | 1104/1105: the one failure is `TextureTile_RenderByteIdentical` (GitHub #45, held until spec 042), the baseline's own |
| CP-d | 915 images, **0 differing, 0 missing**; every `MaxBlockAvgDiff` equal to `baseline-diffs-macos.txt` | 915 images, **0 differing, 0 missing**; `MaxBlockAvgDiff` equal to `baseline-diffs-linux.txt` |
| `^determinism$` | 156/156 | 156/156 |
| determinism sweep (`-L '^determinism-sweep$' -j2`) | **419/419; 419 images, 0 differing, 0 missing** against `baseline-sweep-images-macos.sha256` | **419/419; 419 images, 0 differing, 0 missing** against `baseline-sweep-images-linux.sha256` |
| `Motion_*` (`CameraSlerpEquivalence`, `Replay_*`, `FrameParamsEqualCore`) | 13/13; `git diff --stat e83d005d -- tests/unit/motion/replay` empty | 13/13 |
| `^geometry_contract$` | 5/5 | 5/5 |
| `-L architecture` | 31/31; 173 entries, `RESULT PASS` | 31/31 |
| `-L preview` / `plugin_abi` / `state` / `texture_tile -j2` / `common` / `link` | 20/20, 71/71, 7/7, 15/15, 18/18, 4/4 | `plugin_abi` 71/71, `state` 7/7 (Phase 6 run; unchanged code since) |
| `-L shading_parity` | 135/136 on the first run (see below), then 2/2 and 6/6 repeats | 136/136 (Phase 6 run) |

**The one anomaly.** In the `-L shading_parity` run, `ShadingParity_JitPLEngaged_trace_samples_cone_slo` ended in a `Timeout` after 482 s (its `_rslo` sibling takes 22 s). It did not reproduce: the same test passed alone (22.1 s), passed in the 1104-test suite of this
same tree and in the Phase 7 suite, and passed 6 times in a row with `--repeat until-fail:6` (21.9-23.2 s each; `build-logs/p8-parity-repeat.log`); Linux passed it. The machine was shared with other agents' jobs at that time. It is reported, not accommodated;
a hang, if there is one, is intermittent and out of reach of this phase's evidence (P1 candidate if it recurs).

Timing, `timing_sc010.sh build-base build 5` (`build-logs/p8-timing.log`; start vs final medians, s, spreads large because the machine was shared): `killeroo-reyes` 4.00/3.92, `csg-hero-displaced-raytrace` 11.19/10.60, `blobby-csg-difference-reyes` 4.33/4.15,
`camera-dof-reyes` 4.97/4.78, `camera-dof-raytrace` 5.60/5.78; every difference within the spread, **0 scenes slower beyond it**.

Greps and lists (T105): `grep -rnE 'CReyes|CShadingContext|IShadingProducer|reyes\.h|shading\.h|shader\.h' src/ri/geometry` prints nothing; `ls src/ri/vector/` is `irradianceDispatchStub.cpp`; the allowlist by id is `DB-004 x 3, DB-010 x 8, DB-011 x 151, DB-025 x 11` = 173;
`git diff --stat e83d005d -- tests/unit/motion/replay` and the `tests/**/reference` files are empty (no reference or recording touched); **FR-007** `grep -rnE 'public +(IRayContext|IDicer|IObjectSink)' src/ri/geometry` prints nothing; **FR-019**
`grep -nE 'CSG|Region' src/ri/geometry/{rayContext,dicer,objectSink,geometryFrame}.h` prints nothing, and `CRegionPredicate`, `regionCoverage` and `CCSGSolid`'s entry points (`intersect`, `dice`, `resolve`, `build`) keep their names.

**FR-021 / US6 AS2 note (no member added).** `IRayContext` has exactly these members: `threadIndex()`, `scratch()`, `varyings()`, `shade()`, `displace()`, `saveShadingState()`, `restoreShadingState()` and the non-virtual `frame()`; `IDicer` has `rayContext()`, `drawObject`, `drawGrid`, `drawPoints`,
`probe`, `inFrustum` and `frame()`. Spec 057's surface-sampling request can be added to `IRayContext` as one more pure virtual taking only Geometry-owned and Infra types (a request of points, normals and sample counts, answered by the hider) with no Hiders or Shading type in its
signature; nothing is added now.

T107: `tests/architecture/metrics-baseline.txt` holds Geometry 2/2, Hiders 499/72 and TOTAL 501/74 with dated reasons; `ctest -L architecture` is green against the unedited registry (173, `RESULT PASS`); `git diff --numstat e83d005d -- tests/architecture/allowlist.txt` is `0 171`: no line added, 171 deleted.
The DB-017 re-keys of Phases 4 (32) and 5 (20), 52 lines in all, were deleted in Phase 6 together with the other 25 (77 in all), so against the base the allowlist shows deletions only.
T106: the registry diff is `tools/phase8-registry.diff` (`git apply --check` clean; **not applied**; T109a applies it only on the maintainer's acceptance).

**PAUSE 8 outcome (2026-11-11).** The maintainer accepted the Phase 8 gate and the T106 registry diff `tools/phase8-registry.diff`; T109a applies it next (the lens phase, Phase 9, starts only after the maintainer commits Phase 8).
**T109a done (2026-11-11).** `tools/phase8-registry.diff` applied exactly to `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` (+70/-29: DB-006 moved to Resolved with "spec 031" and 2026-11-11, the DB-004, DB-011, DB-017, DB-025 and DB-009 history entries; DB-004, DB-009, DB-017 and DB-025 stay Open) and staged. `ctest -L architecture` against the edited registry:
**31/31**, archcheck `ALLOWED DB-004 x 3, DB-010 x 8, DB-011 x 151, DB-025 x 11` = 173, `RESULT PASS violations=0 stale=0 badid=0` (`build-logs/p8-arch-reg.log`).

### Phase 10 lens investigation (T130 to T132; 2026-11-11; no `src/` change)

`lens-investigation.md` holds the measurements, the cause and the proposed changes (P1 to P5); scripts and raw outputs are in `tools/lens-investigation/`. Headlines: the default pixel filter is Catmull-Rom width 2 (`options.cpp:285-287`) and a Gaussian of the same width cuts the raytracer's noise by 31 percent and Reyes's by 19 percent at 3x3 samples (tracked scenes, 800x600) and removes the raytracer's non-monotone noise against the sample count; a defocused tight specular highlight is a real, physically correct source of isolated bright samples in the raytracer (not in the matte tracked scenes); no film-lens correlation in the camera slots (|r| < 0.013) and the stratified slots already give 22 to 26 percent over independent sampling. Nothing is changed.
**PAUSE 10 outcome (2026-11-12).** The maintainer approved the report as recommended: no proposal is implemented, **T133 is not applicable** (the decision is to keep the code, the scenes, the filter default and the references as they are), and the finding is filed as GitHub #166 (default Catmull-Rom filter and depth-of-field noise; P1, P2 and P5 as future options), cited in `lens-investigation.md`. `tools/phase10-test-comments.diff` was applied to all five test headers (the four named plus `test_frame_default.cpp`; comment only): the build is exit 0, `^geometry_contract$` 5/5, and `ctest -L architecture` 31/31, 173 entries, `RESULT PASS`, metrics 495/497 (`build-logs/p10-arch2.log`).
`ctest -L architecture`: 31/31, `RESULT PASS`, 173 entries, metrics 495/497 (`build-logs/p10-arch.log`); no source, test or reference changed in the phase.
The comment-only rewording of the stale "OPENRENDER_031_RED_TESTS is ON until its phase enables it" sentence in five `tests/geometry_contract/` test headers is prepared as `tools/phase10-test-comments.diff` (`git apply --check` clean, **not applied**; the maintainer named four files, `test_frame_default.cpp` carries the same sentence and is the fifth).

### Phase 11 documentation proposal (T134 to T136; 2026-11-12; nothing applied)

Prepared for PAUSE 11a: `tools/phase11-docs.diff` (`git apply --check` clean; eight files: `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` (DB-009 to Resolved, a DB-011 history entry for 499 to 495, and the `kRendererServicesVersion` sentence of DB-006 reworded so that it no longer fixes 031 at 2.2.0: it steps one MINOR at merge, 2.3.0 after spec 032's 2.2.0), `DEVNOTES.md` (the spec 031 status row and the two D9 sentences), `DEVNOTES_DETAILS/HIDER_PARITY.md`, `RISPEC_GAPS.md`, `REYES_PAPER_COMPARISON.md`, `specs/008-hider-parity-convergence/{spec,tasks}.md` (dated notes, history kept) and `tests/visual/parity-thresholds.md` (a dated section; no threshold changed)) and `claude-md-draft.md` (gotchas 13, 14, 17, 19, 26, 28 and 35 reworded, one new gotcha numbered "next free at merge preparation", Testing, Active Technologies and Recent Changes; `CLAUDE.md` is never edited by a task). The DB-004/011/017/025 and DB-006 entries were re-checked against the final allowlist (173) and metrics (495/497): only DB-011's history needed a new entry. `ctest -L architecture`: 31/31, `RESULT PASS` (`build-logs/p11-arch.log`).
**PAUSE 11a outcome and T137 (2026-11-12).** The maintainer approved `tools/phase11-docs.diff` in full (including `tests/visual/parity-thresholds.md`, documentation only) and the content of `claude-md-draft.md`, which is **not applied on this branch**: it is applied at merge preparation with the gotcha number (next free; 030 takes 38) and every "Old" text re-checked against master. `phase11-docs.diff` is applied exactly (eight files, staged). `contracts/geometry-interfaces.md` is updated to the final design (the frame carries `openrender::LensModel lens`, no `cocFactorPixels`; a dated note that the plan changed in the lens phase).

### Phase 11 final validation (T138, T139; 2026-11-12; the tree of commit `613d4a81` plus the approved documentation edits)

| Check | macOS | Linux (GCC 13 `-Werror`, remote cpujob `-j2`, `build-logs/l11-*.log` on the host) |
|---|---|---|
| CP-a build | exit 0, no new compiler warnings | exit 0, 0 warnings |
| everyday run `-LE determinism-sweep` (`-j4` macOS) | **1107/1107** (one self-skip: `LibShader_Format_rslo_lower_minor`) | 1107/1108: the one failure is `TextureTile_RenderByteIdentical` (GitHub #45, held until spec 042) |
| manifest against the baseline | 915 images, **5 differing** (the five approved Reyes depth-of-field images), 0 missing | the same five, 915 images, 0 missing |
| `MaxBlockAvgDiff` against `baseline-diffs-*.txt` | four lines differ: `Parity_dof` 40.45 to 9.92, `Parity_dof-motion` 22.72 to 2.41, the two `Visual_*` DoF tests 14.89 and 16.48 to 0.00 (0.02 on Linux) | the same four lines |
| `^determinism$` (`-j2`) | 156/156 | 156/156 |
| determinism sweep (`-j2`) | **419/419**; 419 images, **5 differing** against the sweep baseline: `top-camera-dof-reyes`, `camera-dof-reyes`, `camera-motion-small+dof-reyes`, `parity-dof-reyes`, `parity-dof-motion-reyes` | **419/419**; the same five |
| `-L architecture` | 31/31, `RESULT PASS`, 173 entries (`DB-004 x 3, DB-010 x 8, DB-011 x 151, DB-025 x 11`), metrics Geometry 2/2, Hiders 495/72, TOTAL 497/74 equal to the baseline | 31/31 |
| `-L preview`, `plugin_abi`, `state`, `texture_tile -j2`, `common`, `link` | 20/20, 71/71, 7/7, 15/15, 18/18, 4/4 | inside the everyday run above (every label but the sweep), all passing except #45 |
| `-L shading_parity` | 136/136 (no `ShadingParity_JitPL*` timeout) | 136/136 |
| `-L '^camera$'`, `-R 'Motion_'`, `^geometry_contract$` | 8/8, 13/13, 5/5 | 8/8, 13/13, 5/5 |
| `Motion_Replay_*` | `git diff --stat e83d005d -- tests/unit/motion/replay` empty | recordings untouched |
| timing, `timing_sc010.sh build-base build 5` (`build-logs/p11-timing.log`) | **0 scenes slower beyond the spread**; medians start/final (s): `killeroo-reyes`, `csg-hero-displaced-raytrace` and `blobby-csg-difference-reyes` within spread (blobby 4.30 vs 4.59), `camera-dof-reyes` 4.77 to 2.99 (-41 percent: the smaller physical blur bound; the script exits 3 only because the final median is under its 3 s guard), `camera-dof-raytrace` 5.41 to 5.40 (-9 percent of medians, within spread) | not run (macOS only) |
| `linux_sync.sh pull` / `verify` | | pulled the 9 Linux-produced files, **0 differences**; `verify`: **3780 files, 0 differences** |

## §2 Red evidence

### Phase 2 (macOS; `build` configured with `-DOPENRENDER_031_RED_TESTS=ON`, then OFF again)

PAUSE 2a (T021) approved the test list, the F1 case in `Geometry_FrameDefault` (recorded in `contracts/tests.md`) and the `blobbyRepeller.cpp` exception keyed by file and token count.
Files written: `tests/geometry_contract/{CMakeLists.txt, contract_test.h, fixtures.h, test_dice_through_dicer.cpp, test_intersect_through_ray_context.cpp,
test_object_sink.cpp, test_frame_default.cpp, test_expansion_through_host.cpp}`; `tests/architecture/{check_geometry_no_names.cmake, check_ribvector_no_stubs.cmake}`;
edits to `tests/CMakeLists.txt` (the option, default OFF, and `add_subdirectory(geometry_contract)`) and `tests/architecture/CMakeLists.txt` (two `add_test`, inside the option block, label `architecture`).
The five programs are drafts against the contracts: they could not be compiled (the headers they include do not exist), so any compile fix needed when they first build lands with the
code of the phase that enables them (Phase 3 for `Geometry_ObjectSink` and `Geometry_FrameDefault`, Phase 6 for the rest) and is shown at that phase's TEST-APPROVAL.

| Test | Result with the option ON (`tools/red-geometry-contract-macos.txt`, `tools/red-architecture-macos.txt`) |
|---|---|
| `Geometry_DiceThroughDicer` | build fails: `'dicer.h' file not found` |
| `Geometry_IntersectThroughRayContext` | build fails: `'rayContext.h' file not found` |
| `Geometry_ExpansionThroughHost` | build fails: `'dicer.h' file not found` (the first missing header; the host also lacks `expandDelayedObject`/`expandDelayedInstance`) |
| `Geometry_ObjectSink` | build fails: `'objectSink.h' file not found` |
| `Geometry_FrameDefault` | build fails: `'geometryFrame.h' file not found` |
| `Geometry_NoHiderShadingNames` | fails: **311 banned-identifier tokens** under `src/ri/geometry/` (each printed as `file:line: token`); the `blobbyRepeller.cpp` exception sees exactly its 2 `CRenderer` tokens (line 138) |
| `Geometry_RibVectorNoStubs` | fails: `ribVector_obj` compiles `vector/geometryDispatchStub.cpp`, `rendererDeclarationsDispatchStub.cpp` and `surfaceCrossingDispatchStub.cpp`; and **55 undefined references** in `nm -u` name `CShadingContext`/`CReyes` (e.g. `CSurface::dice(CReyes*)`, `CSurface::intersect(CShadingContext*, CRay*)`) |

Two defects found and fixed in my own new tests before the evidence was recorded (neither touches an approved test): `test_expansion_through_host.cpp` included `diagnostics.h` without its `common/` prefix; and the first
`Geometry_RibVectorNoStubs` run reported **0** undefined references because `execute_process` truncated the large `nm` output piped into `c++filt` (demangled output 17 bytes). The check now matches the mangled name's
length-prefixed identifiers (`15CShadingContext`, `6CReyes`, `6CHider`, `16IShadingProducer`), exact by construction, and needs no `c++filt`; it was re-run (the table above is the second run).

Switched back: `OPENRENDER_031_RED_TESTS=OFF`; `cmake --build build` finds nothing to rebuild (0 `Building` lines in `build-logs/p2-offbuild.log`).

### Phase 3 scratch mutations (T045; each reverted, `git diff` empty afterwards)

1. **F1 install point.** `beginGeometryFrame()` moved to right after `beginFrameParams(samplingConfig)` (`renderer.cpp:805`), rebuilt, `ctest -R 'Visual_.*raytrace'` (80 tests): **77 of 80 failed and `image_manifest.sh diff` reported 77 differing images** (the 835 "missing" are every image outside the subset, as expected). The install point after the context loop is therefore load-bearing, and the F1 evidence is red as required.
2. **`camera2pixels` copy.** One character changed in `geometryFrame.h` (`- f.pixelLeft` to `+ f.pixelLeft` in the perspective branch): `Geometry_FrameDefault` **fails** (exit 8).
3. **Report check (T039).** See §4 item 6.

### Phase 3 test gate (T035)

PAUSE 3a (T034) approved: the signature-only edits of the seven approved tests and of `CPhonySurface` and `CCaptureHolder`; enabling `Geometry_ObjectSink` and `Geometry_FrameDefault`; the 20 allowlist deletions;
the install point after the context loop; and compile-only fixes to the two enabled tests (no assertion, threshold or expected value). Evidence (`tools/red-phase3-macos.txt`):
- `Geometry_ObjectSink` and `Geometry_FrameDefault` moved out of the red block (`tests/geometry_contract/CMakeLists.txt`); after a reconfigure, both fail to build: `'objectSink.h' file not found`, `'geometryFrame.h' file not found`.
  (A first attempt without the reconfigure said "No rule to make target": a gate artefact of `cmake --build --target` before the CMake regeneration, repeated correctly.)
- The 20 allowlist lines deleted by key; `ctest -L architecture` red with **exactly 20 `VIOLATION` lines** (`ALLOWED DB-004 x 33 ... DB-017 x 77`, `RESULT FAIL violations=20 stale=0 badid=0`): `blobby.cpp -> CRenderer::retrieveVariable`; `csgSolid.cpp:30` and `subdivisionLoop.cpp:40 -> riInterface.h`;
  `-> rendererContext.h` of `curves`, `delayed`, `object`, `patches`, `points`, `polygons`, `quadrics`, `subdivisionCreator`, `surface`; `object.cpp -> CRenderer::{imagePlane, projection, raytracingFlags, dPixeldx, dPixeldy, pixelTop, pixelLeft}`; `object.cpp -> CRenderer::findCoordinateSystem`. (`failing-cycles=1` appeared with them and is gone with the code.)

**Compile-only fixes to the two enabled tests** (the programs had never compiled; shown as diffs, no assertion or threshold touched):

```
tests/geometry_contract/test_object_sink.cpp
-        CCsgSession session;
+        csgtest::CCsgSession session;                      (the helper lives in namespace csgtest)
tests/geometry_contract/test_frame_default.cpp
-        f.raytracingFlags = ATTRIBUTES_FLAGS_PRIMARY_VISIBLE | ATTRIBUTES_FLAGS_SHADOW_VISIBLE;
+        f.raytracingFlags = ATTRIBUTES_FLAGS_PRIMARY_VISIBLE | ATTRIBUTES_FLAGS_TRANSMISSION_VISIBLE;   (no SHADOW_VISIBLE exists; the transmission/shadow bit is the one meant; the test sets a value and reads the same value back)
-        CRenderer::projection = f.projection;
+        CRenderer::projection = (EProjectionType)f.projection;                                        (int to enum)
```

### Phase 6 test gate (T078, run before any source edit; macOS)

PAUSE 6a approved: the five tests enabled (`tests/geometry_contract/CMakeLists.txt`: the three vector-flavour programs out of the red block; `tests/architecture/CMakeLists.txt`: the two checks out of it), the residual list 9 -> 5 lines
(`rendererStatics.cpp`'s cell now `Geometry (blobbyRepeller.cpp), Hiders (compositor.cpp, irradiance.cpp)`), the 118 allowlist lines deleted by key (DB-006 23, DB-004 16, DB-017 77, DB-011 2; 0 added), no test-side include addition
(the probe `tools/include-fallout-phase6.txt` found none). After a reconfigure, the three contract programs **build** (against the stubs) and the run of the seven tests (`build-logs/p6-tg-run.log`) is **7 of 7 red, for the stated reasons**:

| Test | Result |
|---|---|
| `Geometry_DiceThroughDicer`, `Geometry_IntersectThroughRayContext`, `Geometry_ExpansionThroughHost` | built and failed at run time on the stubs' `CODE_BUG`: `CSurface::dice() reached in a build with no rendering pipeline` (x4), `CPoints::dice()`, `CSurface::shade()`, `CDelayedObject::intersect()` (x2)/`dice()`, `CDelayedInstance::intersect()` (x2)/`dice()`; no link failure |
| `Geometry_NoHiderShadingNames` | fails: 24 banned identifiers (comment-only; the list of `tools/include-fallout-phase6.txt`) |
| `Geometry_RibVectorNoStubs` | fails: `ribVector_obj` compiles `geometryDispatchStub.cpp`, `rendererDeclarationsDispatchStub.cpp`, `surfaceCrossingDispatchStub.cpp`, and its `nm -u` names `CShadingContext`/`CReyes` symbols (including **`irradianceSampleAccept(float, float, CShadingContext*)`**, see §4 item 9) |
| `SceneInterface_RibVectorResidual` | fails: 4 failures, `ribVector_obj` compiles `rendererDeclarations.cpp` and the three stubs, which the 5-line list does not name |
| `Architecture_DomainBoundaries` | `RESULT FAIL violations=118 stale=0 badid=0 failing-cycles=4`: exactly the 118 deleted lines (39 include, 79 link = 77 DB-017 + 2 DB-011) |

### Phase 7 pinned-test diff (T093, PAUSE 7b; prepared, **not applied**)

`tests/unit/motion/test_frame_params_equal_core.cpp` gains two `bits(...)` checks after the `fstop` one (the file `specs/031-geometry-hider-agnostic/tools/phase7-frame-params-test.diff`, `git apply --check` clean):

```
         bits(c.name, "fstop", fp.fstop, CRenderer::fstop);
+        bits(c.name, "cocFactorScreen", fp.cocFactorScreen, CRenderer::cocFactorScreen);
+        bits(c.name, "cocFactorSamples", fp.cocFactorSamples, CRenderer::cocFactorSamples);
```

**PAUSE 7a/7b approval (2026-11-11).** The maintainer approved the pinned-test diff `tools/phase7-frame-params-test.diff` (the two `bits(...)` checks for `cocFactorScreen` and `cocFactorSamples`) and the phase plan: the 11 DB-011 lines, the metric lowering,
the two `FrameParams` copy fields (default 0), the site list, and `motion::cameraRotationAt` in place of the inlined `slerpq` + `qtoR`. The diff is applied after this entry; byte-identity is the bar (no `Motion_Replay_*` re-recording, any difference reported).

### Phase 7 test gate (T094; before any source edit)

The pinned-test diff applied (2 lines); the 11 DB-011 lines deleted by key. `ctest -L architecture` on the unchanged Phase 6 build: `RESULT FAIL violations=11 stale=0` (`build-logs/p7-tg-arch.log`), exactly the 11 link lines:
`reyes.cpp -> CRenderer::` `cocFactorScreen`, `cocFactorSamples`, `invFocaldistance`, `cameraHasRotation`, `relRotQ`, `aperture`, `relTrans`; `stochastic.cpp -> CRenderer::` `cameraHasRotation`, `cameraRotationOnly`, `relRotQ`, `relTrans`.
`cmake --build --target test_frame_params_equal_core` fails to compile: `FrameParams` has no member `cocFactorScreen` (`build-logs/p7-tg-build.log`).

### Phase 9 test gate (T110 to T112, T116; lens phase)

**PAUSE 9a approval (2026-11-11).** The maintainer approved, as recommended: the four pinned-test diffs (`tools/phase9-test-camera-lens-model.diff` T113, `phase9-test-camera-generate-ray-only.diff` T114, `phase9-test-frame-params-equal-core.diff` T115, `phase9-test-geometry-frame-default.diff` T115a); the new tests `Camera_LensCircleOfConfusion`, `Camera_LensRayMatchesCoC` and `Hiders_ReyesBucketBoundCoversCoC`; the `src/ri/hiders/reyesLensBounds.h` seam **on condition that it clamps `zMin <= 0` and the bound test has a case for the clamp** (done: `reyes_lens::kMinDepth = 1e-6f`, cases `zMin` in {0, -1, -100, 0.5 kMinDepth}); the boundary "only depth-of-field images may change". `DoF_BlurDiameter_*` is skipped.
`tests/unit/camera/test_frame_default.cpp` (spec 028's `Frame_DefaultValue`) names neither `cocFactorScreen` nor `cocFactorSamples` (grep over `tests/` for `coc` finds only the files listed in the four diffs): no edit.

Tests written (T111): `tests/unit/camera/test_camera_lens_coc.cpp`, `test_camera_lens_ray.cpp` (`add_camera_motion_test` without `ENABLED`: `EXCLUDE_FROM_ALL`, `DISABLED`), `tests/unit/raster/test_reyes_lens_bounds.cpp` (`Hiders_ReyesBucketBoundCoversCoC`, label `hiders;unit`, in the `OPENRENDER_031_RED_TESTS` block for the red evidence; to let `tests/unit/raster` see the option the `option()` line of `tests/CMakeLists.txt` was moved above `add_subdirectory(unit/raster)` for that step only and restored at T116, when the test left the red block).

Red evidence (T112; `build` configured with `-DOPENRENDER_031_RED_TESTS=ON`, `build-logs/p9-red-*.log`):
- `test_camera_lens_coc` and `test_camera_lens_ray` fail to compile: `no member named 'circleOfConfusion' in 'openrender::LensModel'`.
- `test_reyes_lens_bounds` fails to compile: `'reyesLensBounds.h' file not found`.
- By assertion, a scratch seam (`reyesLensBounds.h` expressing the starting tree's `reyes.cpp:631-632` bound, `|1/z - invF| * ip*A*f/(f+A)`, no clamp, in a scratch project outside the tree) built against the same test: `scratch_old_bound` FAILED with 2,220 failed checks, among them 2,200 bucket-bound coverage checks (the physical blur reaches past the `f/(f+A)`-scaled bound, worst on the wide aperture) and the clamp cases (the old expression is not finite or not as large at `zMin <= 0`) (`build-logs/p9-red-scratch-ctest.log`).
- `ctest -L architecture` on the unchanged tree (before any source edit): 31/31 passed (`build-logs/p9-tg-arch.log`).

T116 (diffs applied, lens tests enabled, `Hiders_ReyesBucketBoundCoversCoC` out of the red block; `build-logs/p9-tg-*.log`): `test_camera_lens_model` and `test_camera_generate_ray_only` fail to compile (`no member named 'circleOfConfusion'`), `test_camera_lens_coc` and `test_camera_lens_ray` likewise, `test_reyes_lens_bounds` (`'reyesLensBounds.h' file not found`) and `test_geometry_frame_default` (`no member named 'lens' in 'CGeometryFrame'`); `test_frame_params_equal_core` compiles (the removed checks only name fields that still exist until T120). `OPENRENDER_031_RED_TESTS` was configured back to OFF before the implementation build.

### Phase 9 implementation record (T117 to T121; macOS; Linux in §3)

Sites changed (T119 invariant: only the scatter reads `vertices[9]` signed). `[9` readers: `reyes.cpp` writer (copyPoints, `vertices[9] = lens.circleOfConfusion(z)`), reader in the per-vertex grid bound (`expansion(vertices[9], ...)`, magnitude), reader in the quad bound (`absf(cVertex[9 + ...])` x4, maximum of magnitudes); signed readers, the scatter only: `stochasticQuad.h:563-566` and `:844-847` (`v0d..v3d`, unchanged) and `stochasticPoint.h:319-320` (unchanged); `grep -n '\[9' src/ri/hiders/zbuffer*.h` finds none.
Bounds: object bucket bound (`drawObject`) through `reyes_lens::expandBucketBounds`; grid bound, per-vertex bound, quad bound (`quadCoc` is now a per-axis `SampleExpansion`) and the grid-level per-time-stratum bound through `reyes_lens::expansion`/`maxAbsCoc`. `pixel->jdx/jdy` are `lens * dSampled` (`stochastic.cpp`).
`FrameParams` gained `dSampledx`, `dSampledy` (copies of `CRenderer::dSampledx/y`, `rendererFrame.cpp`), so the new Reyes sites read the frame, not `CRenderer::` (read through the approved seam's arguments); this is an addition beyond the four approved edits (nothing compares it: `Motion_FrameParamsEqualCore` is unchanged).
Removed: `LensModel::cocFactorScreen()`, `FrameParams::cocFactorScreen/cocFactorSamples`, `CGeometryFrame::cocFactorPixels/invFocaldistance` (now `openrender::LensModel lens`), `CSampler::circleOfConfusion`, the `cocPixels`/`cocScreen` macros, `CRenderer::cocFactorPixels/Samples/Screen` and their computation (`renderer.cpp:447` and the `invFocaldistance` statics stay). Added: `LensModel::circleOfConfusion` (inline), `LensModel::fromFrame`, `src/ri/hiders/reyesLensBounds.h`.

| Check | macOS |
|---|---|
| CP-a build | exit 0, 0 compiler warnings (the 20 `ld` lines are the existing macOS 13.0/13.3 deployment-target and duplicate-library notes), `-Werror` (`build-logs/p9-build2.log`) |
| lens and neighbour tests | `Camera_*` (8), `Frame_DefaultValue`, `Hiders_ReyesBucketBoundCoversCoC`, `Geometry_FrameDefault`, `Motion_*` (12): 30/30 first run |
| CP-c `ctest -L architecture` | 31/31, `ALLOWED DB-004 x 3, DB-010 x 8, DB-011 x 151, DB-025 x 11` = 173, no line added, no stale line, `RESULT PASS`; METRIC Hiders 499 -> **495** refs (72 members), TOTAL 501 -> **497** (74), baseline file **not edited** (proposed below) |
| FR-027 | `grep -nE 'CReyes|CHider|CRaytracer|CShadingContext' src/ri/state/camera.{h,cpp}` prints two existing header comments (`camera.h:20`, `camera.cpp:9`: "CFrameCamera reproduces CRaytracer::computeSamples ...", spec 028) and no code; `circleOfConfusion`, `fromFrame` and the lens sampling name no hider type |
| CP-b suite, fresh stamp (`-LE determinism-sweep`) | 1107 tests, 1105 pass; the two failures are the expected `Visual_camera-dof-reyes` (247 blocks over threshold 20, worst 33.31) and `Convergence_dof-reyes` (the top-rung RMSE plateaus at 0.0297 against the old reference), both against the starting tree's references (not yet replaced; T124) |
| CP-d | 915 images, **5 differing**, 0 missing: `tests/convergence/dof-reyes/convergence-out.tif`, `tests/visual/camera-dof-reyes/...`, `camera-motion-small-dof-reyes/camera-motion-small+dof-reyes.tif`, `parity-dof-motion/dof-motion-reyes.tif`, `parity-dof/dof-reyes.tif`: exactly research R11's Reyes images, **no raytrace image and no non-DoF image differs** (`build-logs/p9-manifest-diff.txt`); `MaxBlockAvgDiff` differs from `baseline-diffs-macos.txt` in four lines only: `Parity_dof` 40.45 -> 9.92, `Parity_dof-motion` 22.72 -> 2.41, `Visual_camera-dof-reyes` 14.89 -> 33.31, `Visual_camera-motion-small-dof-reyes` 16.48 -> 38.25 |
| `^determinism$` and `Motion_Replay_*` | in the suite run: green; replay recordings untouched |
| determinism sweep (`-L '^determinism-sweep$' -j2`) | **419/419**; 419 images, **5 differing** against `baseline-sweep-images-macos.sha256`: `top-camera-dof-reyes`, `camera-dof-reyes`, `camera-motion-small+dof-reyes`, `parity-dof-reyes`, `parity-dof-motion-reyes` (all Reyes DoF; `build-logs/p9-sweep-manifest-diff.txt`) |

Proposed `metrics-baseline.txt` lowering (not applied, awaiting the maintainer): Hiders 499 -> 495 and TOTAL 501 -> 497 references (members unchanged at 72 and 74), "lens phase: the Reyes depth-of-field bounds read FrameParams (`dSampledx/y`) and the lens model; `cocFactor*` statics gone".

### Phase 9 checkpoint after the reference replacement (T126, T128; macOS)

`OPENRENDER_031_RED_TESTS` is removed (`tests/CMakeLists.txt` and the header comments of `tests/geometry_contract/CMakeLists.txt`; `grep -rn OPENRENDER_031_RED_TESTS CMake src tests --include=CMakeLists.txt --include=*.cmake` prints nothing; five test sources keep their old header sentence "OPENRENDER_031_RED_TESTS is ON until its phase enables it", comments only, untouched). The build tree's cached `OPENRENDER_031_RED_TESTS=OFF` entry is inert.

| Check | macOS |
|---|---|
| CP-a build (`build-logs/p9-build3.log`) | exit 0, 0 compiler warnings |
| CP-b suite, fresh stamp (`-LE determinism-sweep`, `build-logs/p9-suite2.log`) | **1107/1107** (one self-skip, `LibShader_Format_rslo_lower_minor`, as on the baseline); `-L visual` included: `Visual_camera-dof-reyes` and `Visual_camera-motion-small-dof-reyes` pass with `MaxBlockAvgDiff` 0.00 against their new references, `Parity_dof` 9.92 and `Parity_dof-motion` 2.41, `Convergence_dof-reyes` passes (it plateaued at 0.0297 against the old reference) |
| CP-d manifest against `baseline-images-macos.sha256` | 915 images, **5 differing** (the five approved Reyes DoF images), 0 missing; `MaxBlockAvgDiff` differs from `baseline-diffs-macos.txt` in exactly four lines (`Parity_dof` 40.45 -> 9.92, `Parity_dof-motion` 22.72 -> 2.41, the two `Visual_*` 14.89 -> 0.00 and 16.48 -> 0.00) |
| determinism sweep (`-L '^determinism-sweep$' -j2`), run on the same `orender`/library binaries (the only later changes: a test comment, CMake, references) | **419/419**; 5 differing against `baseline-sweep-images-macos.sha256`, the five Reyes DoF scenes (`top-camera-dof-reyes`, `camera-dof-reyes`, `camera-motion-small+dof-reyes`, `parity-dof-reyes`, `parity-dof-motion-reyes`) |
| `^determinism$`, `Motion_*`, `^geometry_contract$` | in the suite run, green |
| `-L architecture` | 31/31, 173 entries, `RESULT PASS`; metrics Hiders 495, TOTAL 497 equal the lowered baseline (no NOTE or WARNING for these rows) |

## §3 Linux records

Host `juvenal@ubuntu-2404-desktop.v2-labs.io` (GCC 13, `-Werror`, 2 cores), tree `~/Projects/Development/CLI/openrender-031` (checked absent beforehand; `openrender`,
`openrender-029`, `-038`, `cellar`, `shrimp` untouched). The host itself was down earlier in the session (`Network is unreachable`, port 22 closed); back up on the
coordinator's word. Every remote job through the remote `cpujob` at `-j2`; the remote `/home` had 9.9 GB free before the build and 9.4 GB after the baselines.

| Step | Result |
|---|---|
| T016 `linux_sync.sh push` / `verify` | pushed 3,733 files (dry run: no `build*` path and no `.git`, only `.github`/`.gitignore`); **3733 files, 0 differences**; `no-git-ok` (no `.git` in the remote tree) |
| T016 configure (`031-l1-configure`) | `cmake -B build -S . -DCMAKE_BUILD_TYPE=Release` (no extra flags were given), exit 0 |
| T016 build (`031-l1-build`, `-j2`, default 2700 s timeout) | exit 0, **0 warnings** under GCC 13 `-Werror` |
| T017 suite 1 (`031-l1-suite`, `-LE determinism-sweep -j2`) | **1098 tests, 1097 passed, 1 failed** (694 s): `TextureTile_RenderByteIdentical` (GitHub #45, held until spec 042; a recorded Linux starting-tree failure). 1 test skips itself: `LibShader_Format_rslo_lower_minor`. 915 images in `tools/baseline-images-linux.sha256` (the same count as macOS); 389 rows in `tools/baseline-diffs-linux.txt` |
| T017 suite 2 (noise) | the same single failure (685 s); **915 images, 0 differing**; `tools/baseline-image-noise-linux.txt` holds a comment only |
| T017 architecture | `ctest -L architecture`: 29 of 29 passed. `tools/archcheck-base-linux.txt`: `ALLOWED DB-004 x 52 DB-006 x 30 DB-010 x 8 DB-011 x 164 DB-017 x 78 DB-025 x 12` (344), METRIC Geometry 48/15, Hiders 556/81, TOTAL 604/89, `RESULT PASS`: **equal to macOS** |
| T017 `nm -u` | `tools/ribvector-undefined-base-linux.txt` over the 67 `ribVector_obj` objects (`find`; count equals macOS's 67), 783 lines (Linux `nm` prints per-file headers and `U` prefixes, so the file is not line-comparable to macOS's), 18 lines naming `CShadingContext`/`CReyes` |
| T018 sweep 1 (`031-l1-sweep1`, `-j2`) | **419 of 419 passed** (1398 s); 419 images in `tools/baseline-sweep-images-linux.sha256` |
| T018 sweep 2 (noise) | **419 of 419 passed** (1345 s); **0 images differ**; `tools/baseline-sweep-noise-linux.txt` holds a comment only. (029 recorded one flaky bake3d scene on Linux; it did not recur here) |
| `pull` / `verify` | pulled the 9 Linux-produced files (9 files, 0 differences); after `git add` and a second `push`, **`verify`: 3737 files, 0 differences**, no remote `.git` |

### Phase 2 CP-Lb (Linux build check of the CMake change; option default OFF)

After `git add`, `linux_sync.sh push` (3,747 files) and `verify` (**3747 files, 0 differences**); remote reconfigure (`031-l2-configure`) and incremental build (`031-l2-build`, `-j2`): exit 0, **0 warnings** under GCC 13 `-Werror`;
`ctest -L architecture`: **29 of 29 passed**; `-L '^geometry_contract$'`: "No tests were found" (the option is OFF: the five programs are not built, as intended); `-R 'Motion_(Replay|CameraSlerpEquivalence)'`: **10 of 10 passed**.
A final `push`/`verify` after the last edits to the (OFF-gated) check scripts is recorded under the Phase 2 checkpoint in §1.

Linux baseline pass/fail set: everything passes except `TextureTile_RenderByteIdentical` (known, #45). macOS: everything passes. **The Phase 2 gate files of T016 to T018 all exist.**

### Phase 4 CP-Lb (Linux, 2026-11-10; the host was back after a reboot, /home 8.5 GB free)

After `git add`, `linux_sync.sh push` (3,755 files) and `verify` (**3755 files, 0 differences**); remote incremental build (`031-l4-build`, remote `cpujob`, `-j2`): exit 0, **0 warnings** under GCC 13 `-Werror`;
`ctest -L architecture` **29/29**; `-L '^geometry_contract$'` **2/2**; `-R 'Motion_(Replay|CameraSlerpEquivalence)'` **10/10**; archcheck report **identical to macOS** (`ALLOWED DB-004 x 31, DB-006 x 26, DB-010 x 8, DB-011 x 164,
DB-017 x 77, DB-025 x 11` = 317; Geometry 21/7, Hiders 556/81, TOTAL 577/84; `RESULT PASS`). `pull`: 9 Linux-produced files, 0 differences. Not run on Linux (CP-Lb is a build check): the full suite and CP-d (Phase 8).

### Phase 5 CP-Lb (Linux, 2026-11-10)

After `git add`, `linux_sync.sh push` (3,755 files) and `verify` (**3755 files, 0 differences**); remote incremental build (`031-l5-build`, remote `cpujob`, `-j2`): exit 0, **0 warnings** under GCC 13 `-Werror`;
`ctest -L architecture` **29/29**; `-L '^geometry_contract$'` **2/2**; `-R 'Motion_(Replay|CameraSlerpEquivalence)'` **10/10**; `-L shading_parity` **136/136**; archcheck report **identical to macOS**
(`ALLOWED DB-004 x 19, DB-006 x 23, DB-010 x 8, DB-011 x 164, DB-017 x 77, DB-025 x 11` = 302, `RESULT PASS`). The full suite and CP-d on Linux are Phase 8's.

### Phase 7 CP-Lb (Linux, 2026-11-11)

`linux_sync.sh push` (3754 files) and `verify` (**3754 files, 0 differences**); remote incremental build (`031-l8-build`, remote `cpujob`, `-j2`): exit 0, **0 warnings** under GCC 13 `-Werror`; `ctest -L architecture` **31/31**;
`-R Motion_` **13/13** (`Motion_CameraSlerpEquivalence`, `Motion_Replay_*` on its own `linux.txt` recording, `Motion_FrameParamsEqualCore`); `-L '^geometry_contract$'` **5/5**. The full suite and CP-d on Linux are Phase 8's.

### Phase 9 CP-L (Linux, 2026-11-11; build and unit level; the visual and sweep checks wait for the approved references, T127)

Pushed with `linux_sync.sh push` (3,763 files, `verify` 0 differences); configure and build (`-DCMAKE_BUILD_TYPE=Release`, `-j2`, GCC 13 `-Werror`) exit 0, 0 warnings; `Camera_|Frame_Default|Hiders_Reyes|Geometry_|Motion_` 30/30; `ctest -L architecture` green, METRIC Hiders 495, TOTAL 497, `RESULT PASS` (`build-logs/l9-*.log` on the host).

### Phase 9 CP-Lfull (Linux, 2026-11-11; after the reference replacement; GCC 13 `-Werror`, `-j2`, remote `cpujob`)

Pushed (`linux_sync.sh push`, 3,763 files, `verify` 0 differences); `build-logs/l9b-*.log` on the host.

| Check | Linux |
|---|---|
| CP-a build | exit 0, 0 warnings |
| CP-b suite (`-LE determinism-sweep`) | 1107/1108: the one failure is `TextureTile_RenderByteIdentical` (GitHub #45, held until spec 042), the baseline's own; `Visual_camera-dof-reyes` and `Visual_camera-motion-small-dof-reyes` pass the macOS-rendered references with `MaxBlockAvgDiff` 0.02 (threshold 20 and 90; the references are one set, no per-platform copies), `Parity_dof` 9.92, `Parity_dof-motion` 2.41, `Convergence_dof-reyes` passes |
| CP-d manifest against `baseline-images-linux.sha256` | 915 images, **5 differing** (the same five Reyes DoF images), 0 missing; `MaxBlockAvgDiff` differs from `baseline-diffs-linux.txt` in the same four lines as on macOS (the two Parity values equal macOS's) |
| `-L architecture`, `Motion_*`, `^geometry_contract$`, `^determinism$` | 31/31, 13/13, 5/5, 156/156 |
| determinism sweep (`-j2`) | **419/419**; 419 images, **5 differing** against `baseline-sweep-images-linux.sha256`: `top-camera-dof-reyes`, `camera-dof-reyes`, `camera-motion-small+dof-reyes`, `parity-dof-reyes`, `parity-dof-motion-reyes` (R11's Reyes images; no raytrace and no non-DoF image) |

### T020 / PAUSE 1 outcome (2026-11-09)

The maintainer approved Phase 1 as reported: **Q1 (DB-017 re-key in Phases 4 and 5, 32 + 20 lines), Q2 (install `CGeometryFrame` after the context loop) and Q3 (the Linux CP-Lb build check at
Phases 2 to 7) are confirmed**; the baselines do not contradict them. `build-scratch` (T013's scratch copy) was deleted on the maintainer's approval; its result is kept in `tools/include-fallout.txt`.
Phase 1 is complete; the maintainer commits it before Phase 2 starts.

## §4 Findings for filing

1. HEAD/staging mismatch (§0 Orientation): the tasks-stage edits are committed, not staged. Informational.
2. T013: only `hiders/photonMap.cpp` is include fallout; the 57 non-Geometry failures of a naive strip probe are Geometry's own `object.h:97` `CRenderer` use (informational, for whoever repeats the probe).
3. The `rendererShadingServices.cpp:95` caller cited in F4 no longer exists (informational, spec text drift).
5. **T038 writers audit** (the statics copied into `CGeometryFrame`: `numThreads, maxGridSize, maxEyeSplits, flags, raytracingFlags, projection, imagePlane, dPixeldx, dPixeldy, pixelLeft, pixelTop, clipMin, cocFactorPixels, invFocaldistance`).
   Every writer in `src/ri`: `renderer.cpp:201-256, :386-420, :720, :746` (all in `beginRenderer()`/`beginFrame()` before the context loop, hence before the install) and **`hiders/raytracer.cpp:391` and `hiders/photon.cpp:74`**
   (`raytracingFlags |= ATTRIBUTES_FLAGS_PRIMARY_VISIBLE`, in the hider constructors). No writer after `beginFrameParams` other than those two: the task did not stop. `rendererContext.cpp:1150-1719` write `COptions` fields, not the statics.
   **Readers while the frame is not installed**: `CRaytracer::CRaytracer` reaches `CHider(thread)`/`CShadingContext`, the primary bundle and `new float[]`s; `CPhotonHider::CPhotonHider` reaches `CPhonySurface` (`CSurface(a, world)` -> `CObject::CObject`, which reads neither `raytraced()` nor the frame) and `attach()`;
   `CHider`/`updateState()` are Shading. None calls `geometryFrame()` or `raytraced()`; no reader stops the task and the install stays after the loop. The only `geometryFrame()` readers in Phase 3 are `CObject::estimateDicing` (dice time, inside a frame) and `raytraced()` (exempt accessor).
6. **T039 report check.** `geometryFrame()` reports once (`CODE_BUG`) a read with no frame installed in a debug build of the renderer (`OPENRENDER_GEOMETRY_FRAME_REPORT`, defined for `ribRender_obj` only); `raytraced()` uses `geometryFrameRaytracingFlags()`, which never reports.
   The report is compiled out of the Release suite, so a scratch mutation forced it on (`#if 1`), the whole suite ran (1099/1099 passed), and `LastTest.log` was searched: **exactly one report, from `Geometry_FrameDefault` itself**, which reads the default on purpose ("nothing installed: all zero").
   No other test reaches the report. The mutation was reverted (`git diff` empty).
4. `build` is 580 MB, not the 2.6 GB of the main checkout's tree; the `build-base` copy was affordable once the maintainer freed disk.
7. **Cross-spec note (recorded at PAUSE 4a, 2026-11-10; not acted on).** master `0e554fed` carries the GitHub #153 fix (`94c8d798`, "keep a continued shadow ray's end point"), which this tree
   does not have. It touches `src/ri/hiders/trace.cpp` (one hunk in `CHider::traceGroups`: `cRay->t = C_INFINITY;` becomes `cRay->t = bundle->continuationLimit(cRay);`, master `:239`),
   `src/libshader/shading/{bundles.cpp,bundles.h,executeMisc.cpp,shading.h}` (`CRayBundle::continuationLimit()` at `shading.h:~356`, `CTransmissionRay::tEnd`, `CTransmissionBundle::continuationLimit`),
   `tests/visual/CMakeLists.txt` (+69 lines), seven `transmission-dest-*` RIBs and `tests/visual/shaders/transmission_destination_probe.sl`.
   **Phase 4's hunks that meet it**: `trace.cpp` only, and only in the same function: `:210` (`shadingGroups->object->shade(&context, ...)` becomes `shade(*this, ...)`; about 30 lines before master's hunk,
   no overlap, so the merge is textual-clean; `git merge-tree --write-tree HEAD master` at Phase 3's HEAD exits 0 with no conflict) and `:338`/`:350` (`intersect`/`cluster`, in `CHider::traceRay`, far from it).
   No Phase 4 hunk touches `bundles.*`, `executeMisc.cpp` or `shading.h` (031's only `shading.h` edit is Phase 3's `EShadingDim` at `:84-89`; master's is at `:356`). After master is merged into 031
   (before 031 merges): the seven new `transmission-dest-*` images have no entry in `tools/baseline-images-{macos,linux}.sha256` or the `MaxBlockAvgDiff` baselines, so CP-d must be re-baselined for
   those images only (from master's own results), and the suite count rises by master's new tests. `Motion_Replay_*` is unaffected (no motion expression moved).

### PAUSE 4a outcome (2026-11-10)

The maintainer approved, exactly as proposed: the test-edit list of T048 (a) including the finding that `tests/unit/geometry/test_degenerate_fix_offset.cpp:70` needs no edit; the `*s.ctx()` / `*ctx`
dereference at call sites that now take a reference (no new `rc()` helper); the `CTesselationPatch::lockTessellation()`/`unlockTessellation()` pair on a static `std::mutex` in `surface.cpp` with
`CRenderer::tesselateMutex` removed; `numThreadsAtBuild` set from `geometryFrame()`. The DB-017 names are confirmed from archcheck's STALE/VIOLATION pairs at T060; an approved-test diff that differs from the list stops the phase.

8. **Cross-spec merge resolution, photonMap and `drawObject` (recorded at PAUSE 6a, 2026-11-10; the maintainer's decision, not acted on in this tree).**
   Spec 032 deleted `src/ri/hiders/photonMap.cpp`: its code now lives in `src/ri/texture/photonStore.cpp` and the Core header `src/ri/render/photonMap.h`, which reads `CRenderer::toWorld`/`fromWorld` in Core.
   032 also deletes the same two DB-011 `photonMap.cpp -> CRenderer::toWorld/fromWorld` allowlist lines, and edits the `rendererStatics.cpp` cell of `ribvector-core-residual.txt` and the Hiders and TOTAL rows of `metrics-baseline.txt`.
   Whichever of 031 and 032 merges second keeps 032's version of photonMap, drops 031's `photonMap.cpp` edit (T079: the `frameParams().camera` reads) and its two DB-011 deletions, and re-measures the Hiders and TOTAL metric rows (and the
   `rendererStatics.cpp` residual cell) on the merged tree. Spec 030 also makes `CHider::drawObject` virtual: after the merge `CReyes::drawObject` must still override both `CHider`'s and `IDicer`'s `drawObject`.

9. **Plan defect found at the Phase 6 gate: `Geometry_RibVectorNoStubs` and T088's grep cannot reach zero while spec 032's `irradianceDispatchStub.cpp` stays.** The `nm -u` rule (approved at PAUSE 2a) lists every undefined symbol of every
   `ribVector_obj` object naming `CShadingContext`/`CReyes`/`CHider`/`IShadingProducer`. Two such symbols belong to Hiders' `irradiance.cpp`, not to Geometry: `irradianceSampleAccept(float, float, CShadingContext*)` (called by `irradiance.cpp:388`,
   defined by `vector/irradianceDispatchStub.cpp`, which the plan keeps for spec 032) and `CIrradianceCache::sample(..., CShadingContext*)`. They were in the starting list too (`tools/ribvector-undefined-base-macos.txt`). The Geometry half of the check
   (no stub source, no Geometry symbol of those types) is satisfiable; the irradiance half is spec 032's. Proposal for PAUSE 6 (no edit made): either exempt exactly those two symbols in `check_ribvector_no_stubs.cmake` with a comment naming spec 032, or leave the check red until 032 merges.
10. **Merge note, the irradiance exemption (maintainer-approved at PAUSE 6).** `tests/architecture/check_ribvector_no_stubs.cmake` exempts exactly two `nm -u` symbols, `22irradianceSampleAccept` and `16CIrradianceCache6sample`, because
   `irradiance.cpp` (Hiders) and `vector/irradianceDispatchStub.cpp` are spec 032's. **The second of 031 and 032 to merge removes this exemption** (the first leaves it, since the stub still exists then).
11. **Merge-overlap notes (maintainer, 2026-11-11).** (a) Spec 032 also steps `kRendererServicesVersion` from 2.1.0 to 2.2.0: whichever of 031 and 032 merges second steps it to 2.3.0 and puts its history entry after the first one's.
    (b) Master moved: spec 056 merged as `11fa3424`. Master is **not** merged into 031 now; that happens at merge preparation (with the #153 note of item 7 and the photonMap/`drawObject` and irradiance-exemption notes of items 8 to 10).
12. **Parity timeout acknowledged (2026-11-11).** The maintainer acknowledged the one-off `ShadingParity_JitPLEngaged_trace_samples_cone_slo` timeout of the Phase 8 run (it did not reproduce in 9 reruns); if it recurs it is reported as a P1 candidate.

## §5 Reference approvals

### Phase 9: the changed depth-of-field images (T122, T123; **no reference changed before PAUSE 9e**)

Measured with `Option "sampling" "seed" [1]` copies of each scene (seed 1 against seed 0, same build) for the noise, and the new build against the starting tree's render at seed 0 for the change; `MaxBlockAvgDiff` over 8x8 blocks (scratch `031-p9-noise.sh`, `build-logs/p9-noise.log`):

| Scene | noise before (seed 1 vs 0) | noise after | after vs before | Reyes vs raytrace before -> after |
|---|---|---|---|---|
| `camera-dof-reyes` (= `parity/dof-reyes`) | 11.00 | 11.55 | **33.39** | 40.45 -> **9.92** (raytrace noise 13.81) |
| `camera-motion-small+dof-reyes` | 12.59 | 13.38 | **38.27** | 44.08 -> **11.06** (14.67) |
| `parity/dof-motion-reyes` | 4.06 | 3.41 | **20.69** | 22.72 -> **2.41** (3.56) |
| the raytrace twins (4) | 13.81, 14.67, 13.81, 3.56 | identical | **0.00** (byte-identical) | |

The Reyes change is 3 to 5 times its own noise, and after the change Reyes agrees with the raytracer within the raytracer's own noise (US7 AS2): `Parity_dof` 40.45 -> 9.92, `Parity_dof-motion` 22.72 -> 2.41. `Convergence_dof-reyes` fails against the old reference (top-rung RMSE 0.0297 does not fall: the sampler converges to the new image).
Implied blur, `DepthOfField 2 1 5` (aperture radius 0.25, focus 5, fov 45: image plane 2.414, 300 pixels per screen unit): radius in pixels `181.1 * (1/5 - 1/z)`: z = 3: -24.1 (near field, mirrored), 4: -9.1, 5: 0, 6: +6.0, 8: +13.6, 10: +18.1, infinity: +36.2; the old Reyes radius was `sqrt(2) f/(f + A) = 1.347` times that.
#112 (`gh issue view 112`, read only): `coverage-alpha-sides2-tjunction-r0.5_reyes` is static and non-DoF, so the lens phase cannot touch it; its image has the baseline hash `65e786c887f3a541` in the suite manifest, and in 12 repeated runs of the `CoverageAnalytic_alpha` family at `-j4` on each of `build-base` and `build` (24 runs) every run gave that hash; the dropout did not reproduce, and nothing here claims a fix.
**PAUSE 9e approval (2026-11-11).** The maintainer approved all five changed images (`visual/camera-dof-reyes`, `camera-motion-small+dof-reyes`, `parity/dof-reyes`, `parity/dof-motion-reyes`, `convergence/dof-reyes`) as recommended, the regeneration of `convergence/dof-reyes.tif` through the README's mechanism, the lowering of `metrics-baseline.txt` (Hiders 499 -> 495, TOTAL 501 -> 497, members unchanged), the comment-only fix of the stale `cocFactorPixels` mention in `tests/geometry_contract/test_frame_default.cpp`, and **keeping `FrameParams::dSampledx/dSampledy` as an approved addition** (copies of the core's `CRenderer::dSampledx/y`, `rendererFrame.cpp`; nothing compares them).

### Phase 9 reference replacement (T125; 2026-11-11)

Only the approved references changed; no threshold touched. Parity tests store no reference.
- `examples/rib/tests/references/camera-dof-reyes.tif` and `camera-motion-small+dof-reyes.tif`: each replaced by the image the `Visual_*` test itself wrote in the CP-b run (`build/tests/visual/camera-dof-reyes/camera-dof-reyes.tif`, sha256 `05fb12b243f0...`, and `camera-motion-small-dof-reyes/camera-motion-small+dof-reyes.tif`, sha256 `915ab47f64d5...`, equal to the CP-d manifest entries; the test's own command: `test_visual_render <orender> <rib> <output_tif> <reference> <threshold>` in `build/tests/visual/<scene>`, with the `VISUAL_ENV` environment).
- `tests/convergence/reference/dof-reyes.tif`: regenerated with the README command for that scene only (`test_convergence --make-reference --orender build/src/orender/orender --template tests/convergence/scenes/dof-reyes.rib.in --reference tests/convergence/reference/dof-reyes.tif --knob spp --ref-knob 256 --ref-renders 256 --scratch build/tests/convergence/dof-reyes-reference`, `SHADERS`, `ORENDERHOME`, `DISPLAYS`, `GEOMETRIES` set as the test sets them, through `cpujob`; mean of 256 renders at 256 spp, independent sampler, 57 s): `build-logs/p9-convref.log`.

## §6 FR/SC evidence table

Final status (Phase 11, T140; 2026-11-12). Every requirement and success criterion has its evidence and status; `proven` rows are backed by the named test or grep on macOS and on Linux (final runs in §1 "Phase 11 final validation" and §3). The only images that differ from the starting tree's baselines are the five approved Reyes depth-of-field renders (SC-001's lens part).

| Requirement | Evidence | Status |
|---|---|---|
| FR-001 | `Geometry_NoHiderShadingNames` (comments included) scans `src/ri/geometry`; the 30 DB-006 lines are deleted; the greps of Phase 8 and `grep -rnE 'CReyes\|CShadingContext\|IShadingProducer\|reyes\.h\|shading\.h\|shader\.h' src/ri/geometry` print nothing | proven |
| FR-002 | `IDicer` (`dicer.h`), `CGeometryFrame` (`geometryFrame.h`); `Geometry_DiceThroughDicer`, `Geometry_FrameDefault` | proven |
| FR-003 | `IRayContext` (`rayContext.h`), the tessellation lock local to `surface.cpp`; `Geometry_IntersectThroughRayContext` | proven |
| FR-004 | `CGeometryHost` expansion (`expandDelayedObject`/`Instance`); `Geometry_ExpansionThroughHost` | proven |
| FR-005 | `common/shadingDim.h` (Infra) | proven |
| FR-006 | the forwarding entries and `IShadingProducer` retired; `kRendererServicesVersion` stepped one MINOR (2.2.0 on this branch; **2.3.0 at merge**, after spec 032's 2.2.0) | proven (number renumbered at merge) |
| FR-007 | `grep -rnE 'public +(IRayContext\|IDicer\|IObjectSink)' src/ri/geometry` prints nothing; the interfaces are abstract and in the Geometry directory | proven |
| FR-008 | the `geometryDispatch.cpp` bodies live in `src/ri/geometry`; the file and its stub are deleted | proven |
| FR-009 | `surfaceCrossings.cpp` is one implementation compiled for both libraries; its dispatch pair is gone | proven |
| FR-010 | `rendererDeclarationsDispatch.cpp` and its stub deleted; `findCoordinateSystem` through `CGeometryHost` | proven |
| FR-011 | `Geometry_RibVectorNoStubs` (with the approved two-symbol irradiance exemption of spec 032's stub), `SceneInterface_RibVectorResidual`; residual list 9 to 5 | proven |
| FR-012 | Reyes and the stochastic rasterizer read `FrameParams` and rotate through `motion::cameraRotationAt`; `Motion_CameraSlerpEquivalence` | proven |
| FR-013 | no `CRenderer::aperture/invFocaldistance/cocFactor*` read in Reyes (Phase 7, 11 DB-011 lines deleted; later the lens function) | proven |
| FR-014 | `Motion_Replay_*` (12,943 calls) unchanged and green; `git diff --stat e83d005d -- tests/unit/motion/replay` empty | proven |
| FR-015 | Geometry reads the frame, the host and `CDeclarations`; Geometry `CRenderer::` metric 48 to 2 references, 15 to 2 members | proven |
| FR-016 | DB-004 is 3 lines, all `blobbyRepeller.cpp` (`locateFile`, `texturePath`, the `renderer.h` include) | proven |
| FR-017 | allowlist lines of every resolved or reduced entry deleted (173 left); `Architecture_DomainBoundaries` passes; DB-006 and DB-009 Resolved with "spec 031" and the dates (T109a, T137) | proven |
| FR-018 | no task depends on the merge; the merge-time items are listed in the Notes below | proven |
| FR-019 | `grep -nE 'CSG\|Region'` over the four interface headers prints nothing; `CRegionPredicate`, `regionCoverage` and `CCSGSolid`'s entry points keep their names | proven |
| FR-020 | no tolerance widened, no test disabled; the only references re-recorded are the three approved depth-of-field ones (T124, §5); `tests/**/reference` and the replay files untouched otherwise | proven |
| FR-021 | the FR-021 note in §1 Phase 8: no member added for surface sampling | proven |
| FR-022 | one signed `LensModel::circleOfConfusion(z)` (`state/camera.h`) used by Reyes (`vertices[9]`, the scatter, every bound) and Geometry's dicing; the raytracer's `generateRay` pinned to it by `Camera_LensRayMatchesCoC` | proven |
| FR-023 | `Camera_LensCircleOfConfusion`: f-stops, focal lengths, focus and object distances and image planes swept, pinhole and orthographic 0 | proven |
| FR-024 | `Camera_LensRayMatchesCoC`, `Parity_dof` 40.45 to 9.92 and `Parity_dof-motion` 22.72 to 2.41 (§5); orthographic depth of field stays off in both hiders | proven |
| FR-025 | investigated and reported (`lens-investigation.md`: the default Catmull-Rom filter, specular highlights, sample count against blur area; no film-lens correlation); **no improvement made, by the maintainer's decision at PAUSE 10** (GitHub #166) | investigated; changes declined |
| FR-026 | foreground over sharp background and defocused highlights checked in the scratch scenes (`lens-investigation.md` §5); Reyes's smooth defocus kept (Reyes noise unchanged by design, its blur radius now the physical one) | proven (as investigated) |
| FR-027 | `circleOfConfusion`, `fromFrame` and the lens sampling name no hider type: the `grep` prints only two older header comments in `camera.h`/`camera.cpp` and no code | proven |
| FR-028 | `IObjectSink` (`objectSink.h`); `Geometry_ObjectSink` | proven |
| FR-029 | Phase 8 gate: both platforms byte-identical against their own baselines before any lens work; the lens phase then changed the five Reyes DoF images only | proven |
| FR-030 | `^determinism$` 156/156 and the determinism sweep 419/419 on both platforms, `Motion_Replay_*` unmoved; the lens slot (`CSampler::cameraSample`) is untouched | proven |
| FR-031 | `reyesLensBounds.h`; `Hiders_ReyesBucketBoundCoversCoC` (both sides of the focal plane, spanning it, the `zMin <= 0` clamp, infinity) | proven |
| SC-001 | interface phases: every manifest, the `MaxBlockAvgDiff` lists and the sweep byte-identical on both platforms (Phase 8); lens phase: only the five approved Reyes DoF images differ (§1 Phase 9, final runs) | proven |
| SC-002 | `Motion_Replay_*` and `Motion_CameraSlerpEquivalence` 13/13 on both platforms, recordings untouched | proven |
| SC-003 | archcheck: zero Geometry to Hiders or Shading edges; DB-006 Resolved | proven |
| SC-004 | `libRIBVector` builds with none of the three Geometry stubs; the 78 DB-017 lines deleted; residual list 9 to 5 | proven |
| SC-005 | DB-004 52 to 3; Geometry `CRenderer::` metric 48 to 2 references (baseline lowered, with reasons) | proven |
| SC-006 | the 11 DB-011 camera and lens lines deleted plus the two `photonMap.cpp` lines; Hiders `CRenderer::` metric 556 to 495 references | proven |
| SC-007 | allowlist 344 to 173, deletions only (30 + 78 + 49 + 13 + 1 = 171) | proven |
| SC-008 | `-L architecture` 31/31, `-L visual`, `-L shading_parity` 136/136, `-L state` 7/7, `-L plugin_abi` 71/71, `-L '^determinism$'` 156/156, the preview tests 20/20 and the sweep 419/419 on macOS; the Linux counterparts in §3 | proven |
| SC-009 | `Camera_LensCircleOfConfusion` (perspective and orthographic, the stated ranges) and `Camera_LensRayMatchesCoC`; Reyes and the raytracer agree within the raytracer's noise (`Parity_dof` 9.92) | proven |
| SC-010 | each changed reference approved image by image (T124); noise measured before and after by the visual-test noise method (§5); the #112 scene reported without claiming a fix | proven |
| SC-011 | no clarification marker remains; no task depends on the merge of 030, 032, 056 or 031's own (merge-time items only in the Notes) | proven |

### Open items (for the maintainer; none blocks the spec)
1. GitHub #166: the default Catmull-Rom filter and depth-of-field noise (P1, P2 and P5 of `lens-investigation.md` as future options).
2. GitHub #162: archcheck reads stale objects in a build tree (the Phase 4 and 6 deletions were asked for).
3. Known non-passes: the self-skipping `LibShader_Format_rslo_lower_minor`; on Linux `TextureTile_RenderByteIdentical` (#45, held until spec 042); #163 (photon rows) did not appear in these runs.

### DB table (registry, `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`)
| Entry | Result |
|---|---|
| DB-006 | Resolved (spec 031, 2026-11-11): 30 lines deleted |
| DB-009 | Resolved (spec 031, 2026-11-12): one rotation evaluator, one lens |
| DB-004 | still Open: 52 to 3 lines (`blobbyRepeller.cpp`) |
| DB-011 | still Open: 164 to 151 lines; Hiders `CRenderer::` metric 556 to 495 |
| DB-017 | still Open: every allowlist line deleted (78 at the baseline); the irradiance pair (spec 032) keeps the entry open |
| DB-025 | still Open: 12 to 11 lines |

### Notes: what the maintainer does at merge
- Merge master into 031 first (spec 056 `11fa3424`, spec 032, and 030 when it lands): re-baseline CP-d for the new master images (the seven `transmission-dest-*` and any later scene), and re-run both platforms.
- Spec 032 merges before 031, so 031 is the second: **remove the two-symbol irradiance exemption** in `tests/architecture/check_ribvector_no_stubs.cmake`; keep 032's `photonMap` (drop 031's `photonMap.cpp` edit and its two DB-011 lines) and re-measure the Hiders and TOTAL metric rows and the `rendererStatics.cpp` residual cell on the merged tree; after spec 030 makes `CHider::drawObject` virtual, `CReyes::drawObject` must override both that and `IDicer`'s.
- Step `kRendererServicesVersion` to the next MINOR after spec 032's 2.2.0 (2.3.0) with 031's history entry after 032's; the registry text of DB-006 says so.
- Apply `claude-md-draft.md` to `CLAUDE.md` with the next free gotcha number (36 and 37 are taken, 030 takes 38) and re-check every "Old" text against master then.
- Close the issues through `Closes #NN` lines in the merge commit (#37 and the others the maintainer lists).
