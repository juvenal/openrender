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

## §5 Reference approvals

(Phase 9.)

## §6 FR/SC evidence table

Seeded from "Traceability" in tasks.md (T004). Every requirement is `open` after Phase 1: Phase 1 proves none; SC-007's starting value is 344 allowlist entries (§0).

| Requirement | Tasks | Status |
|---|---|---|
| FR-001, SC-003, US2 AS1 | T028 (T078), T084, T091, T105 | open |
| FR-002 | T037, T066, T067, T069 | open |
| FR-003 | T050-T054 | open |
| FR-004, FR-010 | T042, T043, T044, T080, T082 | open |
| FR-005 | T036 | open |
| FR-006 | T068, T070 | open |
| FR-007 | T050, T066, T105 (grep) | open |
| FR-008, FR-009, FR-011, SC-004, US3 | T029 (T078), T080-T083, T086, T086a, T088 | open |
| FR-012, FR-013, FR-014, SC-002, SC-006, US4 | T093-T098 (T094) | open |
| FR-015, FR-016, SC-005, US5 | T039, T043, T044, T055, T084, T086, T107 | open |
| FR-017, US6 | T106, T107, T109a, T134 | open |
| FR-018, FR-020, SC-011 | Standing rules; T141 only reports | open |
| FR-019 | T105 (grep), T052 (generic names kept) | open |
| FR-021, US6 AS2 | T105 (validation.md note); no member added | open |
| FR-028 | T040 (T035) | open |
| FR-022, FR-023, FR-024, FR-027, SC-009 | T110-T112, T116-T121 | open |
| FR-025, FR-026, SC-010 | T123, T124, T130, T131 | open |
| FR-029, FR-030, SC-001 (lens part) | T121, T122, T125-T127 | open |
| FR-031 | T110(c), T119, T121 | open |
| SC-001, SC-007, SC-008, US1 | CP-b/CP-d/CP-c at every phase, T101-T104, T109, T138, T139 | open |
