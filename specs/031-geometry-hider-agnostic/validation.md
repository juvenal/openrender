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

(Phase 9.)

## §6 FR/SC evidence table

Status after Phase 8 (the interface phase). `proven` rows are backed by the named test or grep on both platforms; the lens-phase rows (FR-022 to FR-027, FR-029 to FR-031, SC-009, SC-010 and the lens part of SC-001) wait for Phases 9 to 11.

| Requirement | Evidence | Status |
|---|---|---|
| FR-001, SC-003 | `Geometry_NoHiderShadingNames` (comments included); the 30 DB-006 lines deleted; `grep` of Phase 8 prints nothing; DB-006 registry diff prepared | proven |
| FR-002 | `IDicer` (`dicer.h`), `CGeometryFrame`; `Geometry_DiceThroughDicer`, `Geometry_FrameDefault` | proven |
| FR-003 | `IRayContext` (`rayContext.h`), the tessellation lock local to `surface.cpp`; `Geometry_IntersectThroughRayContext` | proven |
| FR-004, FR-010 | `CGeometryHost::expandDelayedObject/Instance/findCoordinateSystem`; `Geometry_ExpansionThroughHost`; `rendererDeclarationsDispatch` pair deleted | proven |
| FR-005 | `common/shadingDim.h` | proven |
| FR-006 | forwarding entries and `IShadingProducer` retired; `kRendererServicesVersion` 2.2.0 | proven |
| FR-007 | `grep 'public (IRayContext\|IDicer\|IObjectSink)' src/ri/geometry` prints nothing | proven |
| FR-008, FR-009, FR-011, SC-004 | `geometryDispatch.cpp` and the three stubs deleted; `Geometry_RibVectorNoStubs` (with the approved irradiance exemption); `SceneInterface_RibVectorResidual`; residual list 9 to 5 | proven |
| FR-012, FR-013, FR-014, SC-002, SC-006 | `Motion_*` 13/13 on both platforms, recordings untouched; 11 DB-011 lines deleted; no image changed | proven |
| FR-015, FR-016, SC-005 | DB-004 52 to 3 (`blobbyRepeller.cpp`); Geometry metric 48 to 2 references, 15 to 2 members | proven |
| FR-017 | allowlist lines of every resolved or reduced entry deleted; `Architecture_DomainBoundaries` passes; the registry edit is prepared and applies on acceptance (T109a) | prepared |
| FR-018, FR-020, SC-011 | no task depends on the merge; no tolerance widened, no test disabled, no reference or recording re-recorded (`git diff` of `tests/**/reference` and `tests/unit/motion/replay` empty) | proven |
| FR-019 | `grep` of the four interface headers prints nothing; `CRegionPredicate`, `regionCoverage` and `CCSGSolid`'s entry points keep their names | proven |
| FR-021 | the FR-021 note above: no member added | proven |
| FR-028 | `IObjectSink`; `Geometry_ObjectSink` | proven |
| SC-001 (interface phases), SC-007, SC-008, US1 | CP-b/CP-d/CP-c at every phase and the Phase 8 runs above: both platforms, every manifest and the sweep byte-identical; allowlist 344 to 173 (deletions only) | proven |
| FR-022 to FR-027, SC-009, SC-010 | Phases 9 to 11 | open (lens phase) |
| FR-029 | this gate | proven once the maintainer accepts |
| FR-030, FR-031, SC-001 (lens part) | Phases 9 to 11 | open (lens phase) |
