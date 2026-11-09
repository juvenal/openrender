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
| 1 | macOS build exit 0, 0 warnings; Linux build exit 0, 0 warnings | macOS 1097/1097 (1 self-skip), twice, sweep 419/419 twice; Linux 1097/1098 (#45) twice, sweep 419/419 twice | `ctest -L architecture`: 29 of 29 on both hosts (344 entries, RESULT PASS) | baselines recorded (0 noise on both hosts) | n/a | n/a | n/a | baseline built and verified (§3) |

## §2 Red evidence

(Phase 2.)

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

Linux baseline pass/fail set: everything passes except `TextureTile_RenderByteIdentical` (known, #45). macOS: everything passes. **The Phase 2 gate files of T016 to T018 all exist.**

### T020 / PAUSE 1 outcome (2026-11-09)

The maintainer approved Phase 1 as reported: **Q1 (DB-017 re-key in Phases 4 and 5, 32 + 20 lines), Q2 (install `CGeometryFrame` after the context loop) and Q3 (the Linux CP-Lb build check at
Phases 2 to 7) are confirmed**; the baselines do not contradict them. `build-scratch` (T013's scratch copy) was deleted on the maintainer's approval; its result is kept in `tools/include-fallout.txt`.
Phase 1 is complete; the maintainer commits it before Phase 2 starts.

## §4 Findings for filing

1. HEAD/staging mismatch (§0 Orientation): the tasks-stage edits are committed, not staged. Informational.
2. T013: only `hiders/photonMap.cpp` is include fallout; the 57 non-Geometry failures of a naive strip probe are Geometry's own `object.h:97` `CRenderer` use (informational, for whoever repeats the probe).
3. The `rendererShadingServices.cpp:95` caller cited in F4 no longer exists (informational, spec text drift).
4. `build` is 580 MB, not the 2.6 GB of the main checkout's tree; the `build-base` copy was affordable once the maintainer freed disk.

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
