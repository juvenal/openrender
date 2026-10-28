# Validation: Camera and Motion Layer

**Plan**: [plan.md](plan.md) | **Quickstart**: [quickstart.md](quickstart.md)

Measured evidence, oldest first. Raw data for §0 is in [tools/](tools/) and in the untracked
`build-028-logs/`.

## §0 Baselines on the starting tree, macOS (2026-10-27, plan stage)

**Tree**: `build-028`, configured from the worktree at master `4eb683b`: Release, Unix Makefiles,
Apple Clang 21 (arm64), vcpkg `arm64-osx-openrender`, `OPENRENDER_ENABLE_JIT=ON`. The worktree held
only the spec documents; no file under `src/` or `tests/` was changed. Every job ran through
`~/.claude/bin/cpujob` at `-j4`. Compile flags for `ribRender_obj`: `-O3 -DNDEBUG -std=c++20
-arch arm64 ... -Werror`, no `-ffp-contract` (Clang's default `on`, research R9).

### §0.1 Build

`cmake --build build-028 --config Release -j4`: clean, exit 0.

### §0.2 Architecture check

`ctest -L architecture`: 21/21 passed. Full report: [tools/archcheck-base-macos.txt](tools/archcheck-base-macos.txt).

```text
archcheck: platform=macos objects=270 sources=185 edges=2419 (include=1506 link=913)
ALLOWED   DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-010 x 35 DB-011 x 173 DB-015 x 10
          DB-017 x 78 DB-019 x 3 DB-021 x 1 DB-025 x 12 DB-026 x 3
METRIC    Geometry 78/19, Hiders 567/84, Infra 0/0, SceneInterface 6/6, Shading 0/0,
          Texturing 70/16, TOTAL 721/110 (all equal to metrics-baseline.txt)
RESULT    PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0
```

DB-011's allowlist header says `(182)`, but the file holds **175** DB-011 lines at `4eb683b` (9
include, 166 link; two link lines are tagged `linux` and two `macos`, the thread-local
`activeContext` pair), so macOS counts 173 (175 minus the two Linux-tagged lines); Linux is expected to count 173 as
well (175 minus the two macOS-tagged lines), measured at T037. The header (182) and the registry text's "180 edges per platform" are both stale
(FR-023). After spec 028's 7 deleted lines the file holds 168 and each platform 166 (9 include, 157
link); T102 sets the header to `(168)`.

### §0.3 Full suite and the image manifest

`ctest -LE determinism-sweep -j4`, twice:

| Run | Result | Real time |
|---|---|---|
| 1 | 100% of 838 passed; `LibShader_Format_rslo_lower_minor` skipped by the test itself | 281 s |
| 2 | 100% of 838 passed; same skip | 242 s |

- **Manifest**: run 1 recorded **761 images** ([tools/baseline-images-macos.sha256](tools/baseline-images-macos.sha256)).
- **`MaxBlockAvgDiff` table**: 389 tests ([tools/baseline-diffs-macos.txt](tools/baseline-diffs-macos.txt)),
  identical in both runs.
- **Run-to-run noise**: 759 of 761 images identical. Two differ, both shared scratch outputs of
  `shading_parity` tests writing into one working directory under `-j4` (the last writer wins), not
  test products: `tests/shading_parity/test.tif` (written by `test_matrix_transform_jit`,
  `test_printf_jit`, `test_uniform_conditional_jit`) and `tests/shading_parity/ri.tif` (the tests
  that render with the default display name). Both are in
  [tools/baseline-image-noise-macos.txt](tools/baseline-image-noise-macos.txt); spec 025's T006a
  fixed the same kind of race for `tests/imager/ri.tif` (plan, open question Q5).

### §0.4 Camera, motion and DOF tests present

113 registered tests match camera, motion, DOF, lens or shutter, 57 of them determinism-sweep
scenes. The non-sweep ones are listed in the spec's Measured starting point; all passed in §0.3.

### §0.5 Timing of the three slowest camera, motion and DOF raytrace tests (SC-008)

Each run alone, five times, through `cpujob`:

| Test | Runs (s) | Min | Median | Max |
|---|---|---|---|---|
| `Visual_teapot-motion-raytrace` | 20.29, 20.36, 19.98, 20.10, 27.53 | 19.98 | 20.29 | 27.53 |
| `Convergence_dof-raytrace` | 7.38, 7.45, 7.29, 7.94, 7.20 | 7.20 | 7.38 | 7.94 |
| `Visual_blobby-motion-grow-raytrace` | 3.11, 3.11, 2.92, 2.96, 3.19 | 2.92 | 3.11 | 3.19 |

**Re-timing of `Visual_teapot-motion-raytrace`** (five more runs, after spec 027's determinism sweep
released the lock): 24.28, 25.00, 23.27, 27.53, 31.11 s (min 23.27, median 25.00, max 31.11). The
two series (median 20.29 against 25.00) disagree by more than either series' spread. The lock
serializes only `cpujob` jobs, so this scene's wall time follows machine load and the thermal state
of the fanless M2, not just the code. **A plain before/after band is not reliable for this scene.**
SC-008 is therefore measured **interleaved** at phases 4 and 11: alternate starting-tree and
current-tree runs (a starting-tree build kept in a scratch build directory), five pairs, and
compare the medians of the pairs (the method spec 039's SC-010 used). `Convergence_dof-raytrace`
and `Visual_blobby-motion-grow-raytrace` were stable (spread under 10%).

### §0.6 Still to measure

- macOS replay file and its mutations (phase 1, quickstart §0).
- Linux: the whole of §0 plus the replay file (quickstart §4a), by the maintainer.

## §1 Implementation log

**Rule: no known-red test.** No test is red at any checkpoint; a new test whose API does not exist
yet is `EXCLUDE_FROM_ALL` + `DISABLED` and reports "Not Run (Disabled)". **No phase changes output**:
every image of the suite is byte-identical to the starting tree's from phase 3 on (CP-d).

| Item | Value |
|---|---|
| Base (starting tree) | master `4eb683b` |
| Worktree HEAD at the start of the implement stage | `dc14044f9b4fe22617fa9b0f98cc3f241b0f010` (plan commit; spec `8076a15`); the implement stage's changes are staged, never committed |
| Build tree | `build-028` (Release, JIT on, vcpkg `arm64-osx-openrender`), logs in `build-028-logs/` |
| Scratch tree `X` | `build-028-scratch` (gitignored, inside the worktree): `X/tree` is the copy, `X/bld` its build |

### Checkpoint results

| Phase | CP-a build | CP-b suite | CP-c `ALLOWED` / `METRIC` / `RESULT` | CP-d (differing, missing, MaxBlockAvgDiff) | `-L '^motion$\|^camera$'` |
|---|---|---|---|---|---|
| 1 | clean (`-Werror`); T003 on the unchanged tree: reconfigure only, 0 compiled | 847/847 passed (838 + 9 new `motion`/`motion-record` tests), 250 s | `DB-004 x 65`, `DB-011 x 173`; Geometry 78/19, Hiders 567/84, TOTAL 721/110; `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; 21/21 `architecture` tests | 761 images: **0 differing, 0 missing, 0 not in the manifest**; `diffs` table identical | 8/8 `Motion_Replay_*` |
| 2 | clean (`-Werror`); `build-028` has the option OFF | 847/847 passed (the 11 new tests are not run: 10 do not exist with the option OFF, `Frame_ParamsEqualCore` reports "Not Run (Disabled)" and does not fail the run), 183 s | `DB-004 x 65`, `DB-011 x 173`; Geometry 78/19, Hiders 567/84, TOTAL 721/110; `RESULT PASS`; 21/21 `architecture` tests | n/a (no `src/` change) | 8 replay tests green, `Frame_ParamsEqualCore` Not Run (Disabled) |
| 3 | | | | | |
| 4 | | | | | |
| 5 | | | | | |
| 6 | | | | | |
| 7 | | | | | |
| 8 | | | | | |
| 9 | | | | | |
| 10 | | | | | |
| 11 | | | | | |
| 12 | | | | | |

### Phase 1 log

**Checkpoint 1 decisions (maintainer, 2026-10-27).**
1. The access bypass is approved: the replay tool reaches the private `CCurveMesh::create()` and `CPoints::prep()` through the explicit-instantiation idiom (contracts/replay-format.md rule 1 names it).
2. The tool-building edits are approved (scene table, ray footprints, per-leaf rays, `dPdtime` pinned at BEGIN only, T006, T013, T014, T096) on the condition that the tracked replay shrinks: done, 3.69 MB -> 1.62 MB (see T014), every family's coverage and the nine mutations kept.
3. T096: the mixed-precision lerp at `bsplinePatchgrid.cpp:233-236` stays in place (its `dPdtime` scale goes through `toShutterRate`); for a later precision-unification phase.
4. The out-of-bounds `dPdtime` read of `CPatchGrid::sample` / `CBSplinePatchGrid::sample` is GitHub #110.

**T002, baselines intact.** `image_manifest.sh` executable; `baseline-images-macos.sha256` 761 lines, `baseline-diffs-macos.txt` 389 lines, the noise list names exactly
`tests/shading_parity/ri.tif` and `test.tif`, `archcheck-base-macos.txt` shows `DB-004 x 65`, `DB-011 x 173`, `RESULT PASS`; `git diff --stat 4eb683b -- src tests CMakeLists.txt CMake shaders examples` was
empty at the start of the stage; `build-028` is Release, JIT ON, `arm64-osx-openrender`. At the end of Phase 1, `git diff --stat 4eb683b -- src` is still empty.

**T003.** `cmake --build build-028` on the unchanged tree: exit 0, 0 files compiled (the configure step re-ran because `tests/CMakeLists.txt` had been touched; the production objects were up to date).

**T004 to T013, the replay tool** (`tests/unit/motion/`): `motion_replay.cpp`, `replay_capture.h`, `fixtures/{implicit_moving_sphere,dlo_sphere}.cpp`, `CMakeLists.txt`, `replay/macos.txt`.
18 scenes, 8 families. Built with `openrender_common_flags` (`-Wall -Wextra -Wpedantic -Werror`), clean. clang-format applied to the new files (they were new); the scene table is fenced with `clang-format off`.
Findings made while building it (each one changed the tool, none changed `src/`):
1. `CSurface::intersect` only builds the tessellation child; a ray's differential selects the tessellation level, and a zero footprint asks for the finest level: 60 000 to 840 000 objects visited per ray and the record test timed out at 300 s.
   Rays now carry a fixed orthographic footprint cycling over four widths (0.05, 0.4, 1.6, 6.4), which also reaches every tessellation density of `CTesselationPatch::intersect`'s moving branch.
2. `CCurveMesh::create()` and `CPoints::prep()` are private and reached in production only through `dice()` (a Reyes bucket): explicit-instantiation idiom (approved, decision 1).
3. A `PatchMesh` does not make `CPatchGrid`/`CBSplinePatchGrid`; only the subdivision creator does (and only with `interpolateboundary` or a crease). Scenes `subdiv-grid` and `subdiv-cube` replaced the planned `patchmesh`, `bsplinemesh`, `subdiv`.
4. **Existing defect, GitHub #110, not fixed, not pinned**: `CPatchGrid::sample` and `CBSplinePatchGrid::sample` compute `dPdtime` through `vertexData + disp`, which is right only for the BEGIN call; with `PARAMETER_END_SAMPLE` it already points at the end keys, and at an interpolated time it points past the scratch array.
   The values differ from run to run (two early `record` runs differed in 18 END records) or crash. The replay requests `dPdtime` only with `PARAMETER_BEGIN_SAMPLE`.
5. `bsplinePatchgrid.cpp:233-236` has a mixed-precision lerp (double left product, float right product) that neither `atTimeD` nor `atTimeF` reproduces (T096, decision 3).

**T014, recording.** `Motion_ReplayRecord` through ctest; **three consecutive runs byte-identical** (`cmp`), and the coverage-instrumented scratch build recorded the same bytes. Shrunk after review: 32 rays per leaf (8x4 lattice, was 64), 3x3 (u, v) samples (2x2 for the two subdivision meshes; was 5x5 and 3x3), two quadrics in the camera scenes.
Final file: **12 943 records, 1.62 MB** (was 27 821 records, 3.69 MB), header `# motion_replay v1 macos master-4eb683b`, sha256 of the frozen `tests/unit/motion/replay/macos.txt`: `79fc6be53d5c9200d35779f0a2f3b9eb7126c08de344242d75019e414be41ef0`.
No image is written (no Display is declared; the default display fails to open `file.dsply` with a message, harmless).
`grep -nE 'frameParams\.h|camera\.h|motion\.h' tests/unit/motion/motion_replay.cpp tests/unit/motion/replay_capture.h` prints nothing.

Per family (records): `quadrics` I 757 hit / 2379 miss, S 1134; `raytransform` I 250 / 1990, S 648; `bilinear` I 92 / 132, S 81; `polygons` I 138 / 310, S 162; `implicit` I 45 / 179; `tessellation` I 354 / 766, S 405;
`emptyshutter` I 119 / 329, S 162; `gridonly` S 2511 (no `I`: curves, points and subdivision patches are not raytraced).
Records that differ between time 0 and time 1 (I / S) per moving scene: quadrics-deform 97 / 63, quadrics-xform 89 / 63, camera-translate 17 / 18, camera-rotate 5 / 18, camera-rotonly 6 / 18, own-motion-camera 15 / 18, dlo-translate 16, dlo-rotate 4, bilinear 15 / 9,
polygons 24 / 18, implicit 12, bicubic 20 / 9, nurbs 39 / 36, empty-shutter 7 / 9; `curves` and `points` select a key and ignore time (BEGIN differs from END in 54 and 9 records); subdiv-grid 48 / 48, subdiv-cube 168 / 168.
The tool's own guard (leaf classes, moving or transform motion present) passes for every scene. Leaf classes: quadrics 7 (all), `CBilinearPatch`, `CPolygonTriangle` + `CPolygonQuad`, `CImplicit`, `CBicubicPatch`, `CNURBSPatch` x4, `CCubicCurve` x2 + `CLinearCurve` x4,
`CPoints`, `CPatchGrid` x12 (+ 4 `CBicubicPatch` leaves excluded from the queries, they belong to the `tessellation` family), `CSubdivision` x24 + `CBSplinePatchGrid` x18, `CDLObject`.

**T015.** `build-028/tests/unit/motion/replay-recorded-macos.txt` copied to `tests/unit/motion/replay/macos.txt` (starting tree, once; the shrink re-recorded it before the freeze); 8 `Motion_Replay_<family>` registered (label `motion;unit`, `TIMEOUT 120`): **8/8 green by construction**.

**T016, replay mutations**, in `build-028-scratch/tree` (rsync of the worktree with the anchored excludes; `X/bld` configured like `build-028`; the unmutated scratch tree is green 8/8 and records the same bytes). Each: edit, rebuild `motion_replay`, `ctest -R '^Motion_Replay_'`, revert (`diff -r $W/src $X/tree/src` empty at the end, `CMakeLists.txt` identical). Exactly the named test turned red in all nine, the other seven stayed green. Run against the shrunk replay:

| # | Family | Mutation | Red test | First differing record (replay -> now) |
|---|---|---|---|---|
| 1 | quadrics | `CSphere::intersect` (`quadrics.cpp:200`): drop `(double)` of `nextData[0] * (double)rv->time` | `Motion_Replay_quadrics` (21 of 4270 records) | `quadrics-deform quadrics I 0 2 0x1.4p+0`, `t` `0x1.30353ap+1` -> `0x1.30353cp+1` |
| 2 | raytransform | `objectMisc.h:209`: drop the conjugate of `relRotQ_inv` | `Motion_Replay_raytransform` (51 of 2888) | `quadrics-camera-rotate ... I 0 16 -0x1p-2`: hit -> miss |
| 3 | bilinear | `patches.cpp:228`: swap the operands of `interpolatev(t0, P00, P00 + vertexSize * 4, ...)` | `Motion_Replay_bilinear` (88 of 305) | `bilinear I 0 2 -0x1p-2`: miss -> hit |
| 4 | polygons | `polygons.cpp:183`: swap the operands of the first `interpolatev` | `Motion_Replay_polygons` (51 of 610) | `polygons I 1 2 0x1.8p-1`, `t` `0x1.79b87ap+0` -> `0x1.7dae1ep+0` |
| 5 | implicit | `implicitSurface.cpp:142`: `ray->time` -> `0.0f` | `Motion_Replay_implicit` (31 of 224) | `implicit I 0 2 -0x1p-2`, `t` `0x1.6f5096p+1` -> `0x1.6f509p+1` |
| 6 | tessellation (intersection) | `surface.cpp:1461` (div 1x1), `:1490` (div <= 4 subsampled), `:1512` (2x2 or 4x4), `:1554` (> 4 subsampled), `:1597` (> 4): swap the operands of the first `interpolatev` of each branch; all five run | `Motion_Replay_tessellation` each time (95, 123, 13, 58, 40 records) | `:1461`: `bicubic I 0 3 -0x1p-2`, `t` `0x1.8d74e8p+0` -> `0x1.88f21ep+0`; `:1490`: `bicubic I 0 1 0x1p-2`, `t` `0x1.ba2a2ap+0` -> `0x1.b9f55ep+0` |
| 7 | tessellation (sample) | `patches.cpp:692` (`CBicubicPatch::sample`): swap `vertex0`/`vertex1` in the lerp | `Motion_Replay_tessellation` (54 of 1525) | `bicubic tessellation S 0 0 -0x1p-2`, `P.x` `-0x1.5dc28ep+0` -> `-0x1.44ccccp+0` |
| 8 | gridonly | `curves.cpp:385`: `CRenderer::invShutterTime` -> `1.0f` | `Motion_Replay_gridonly` (18 of 2511) | `curves gridonly S 0 0 BEGIN`, a `dPdtime` component halves |
| 9 | emptyshutter | `quadrics.cpp:486` (sphere `sample()`): `invShutterTime` -> `1.0f / (shutterClose - shutterOpen)` | `Motion_Replay_emptyshutter` (9 of 610) | `empty-shutter emptyshutter S 0 0 BEGIN`, `dPdtime` -> `inf` |

Scene corrections that came out of the first mutation round (before the freeze): mutation 7 had also turned `gridonly` red because `subdiv-grid` holds four `CBicubicPatch` interior faces (now excluded from its queries), and mutation 6 at `:1461`/`:1490` was green
because only one footprint reached the 1x1 and subsampled-4x4 densities (now four footprints). No named substitute mutation was needed.

**Line coverage of the replay** (instrumented scratch build, `llvm-cov`, one `Motion_ReplayRecord` run, repeated on the shrunk replay): all 24 `CRenderer::invShutterTime` reads in `src/ri/geometry` execute (quadrics 14, patches 3, polygons 2, curves 2, points 1, patchgrid 1, bsplinePatchgrid 1),
and no line of `quadrics.cpp`, `patches.cpp`, `polygons.cpp`, `curves.cpp`, `points.cpp`, `patchgrid.cpp`, `bsplinePatchgrid.cpp`, `subdivision.cpp`, `implicitSurface.cpp`, `dlobject.cpp` or `objectMisc.h` that mentions a time (`ctime`, `->time`, `interpolatev`, `timev`, `invShutterTime`, `transform(oFrom`)
has a zero count. The only zero-count time lines are `surface.cpp:284, 298, 316` (the dicing probes, spec 031's).

**T017.** `X/bld` built `all` from the unmutated copy. Timings, one run each through `cpujob`: `Visual_teapot-motion-raytrace` 21.26 s, `Convergence_dof-raytrace` 7.08 s, `Visual_blobby-motion-grow-raytrace` 2.41 s
(plan-stage figures: 20.29 / 7.38 / 3.11 s medians). `X/tree/src` is not edited again. `tests/` was re-synced into `X/tree` after each tool change; the unmutated scratch tree passes 8/8.

**T018.** `build-028` with the Phase 1 tests (measured before the shrink; the shrink changed only the tool and the replay file, no `src/` and no image): CP-a clean; CP-b 847/847 (`build-028-logs/p1.LastTest.log`); CP-d 761 images, 0 differing, 0 missing, 0 not in the manifest, the `MaxBlockAvgDiff` table identical to `baseline-diffs-macos.txt`;
CP-c 21/21 `architecture` tests and the report equal to the baseline (`ALLOWED`, `METRIC`, `RESULT PASS`). `ctest -L '^motion$|^camera$'`: the 8 replay tests green after the shrink (the `motion-record` test carries its own label). T034 re-runs CP-a to CP-c on the tests-only tree.

## §2 Red evidence (phase 2, T033)

**The tests** (11 new, all written against contracts/camera.md, frame-params.md, motion-evaluator.md and data-model.md; shared helper `tests/unit/camera/frame_test_utils.h`):

| Test | File | Kind | Covers |
|---|---|---|---|
| `Camera_Analytic` | `tests/unit/camera/test_camera_analytic.cpp` | State-only | US1 AS1-AS3: closed-form pinhole centre ray, orthographic ray, thin-lens ray through the focal point |
| `Camera_StartingTreeEquivalence` | `.../test_camera_equivalence.cpp` | State-only | FR-006/FR-014: `generateRay` against a verbatim copy of `computeSamples` + `fullImagePixels2camera`, `memcmp`, 3 windows x (persp pinhole, persp thin lens, ortho) x 72 film positions x 8 lens points x 3 times |
| `Camera_CropInvariance` | `.../test_camera_crop.cpp` | State-only | US1 AS4: crop origin does not enter a ray; screen-window origin does |
| `Camera_GenerateRayOnly` | `.../test_camera_generate_ray_only.cpp` | State-only | FR-009/SC-010/US5: a camera with only `generateRay` is valid; `sampleRay` forwards; pinhole default; reserved operations `NotSupported`, outputs untouched |
| `Camera_LensModel` | `.../test_camera_lens_model.cpp` | State-only | FR-007/US5 AS1: lens fields, `thinLens()`, `cocFactorScreen()` vs a copy of `renderer.cpp:447` |
| `Frame_DefaultValue` | `.../test_frame_default.cpp` | State-only | the default `FrameParams`, install/uninstall, no `CSamplingConfig` field repeated (FR-002, compile-time) |
| `Motion_TwoKeyModel` | `tests/unit/motion/test_motion_two_key.cpp` | State-only | FR-011/decision 3: one model, `atTimeD/F/V` bitwise against verbatim copies (quadric, `interpolatev`, subdivision, polygon fused), `keyAtTime` identity, `sampleKey`, `toShutterRate` for 4 shutters |
| `Motion_Isolation` | `.../test_motion_isolation.cpp` | State-only, narrow path | FR-012/US2 AS5/SC-007: `transformRay`, `toShutterRate`, `pluginTime` with no hider, `CRenderer` or shading context in the link line |
| `Motion_CameraSlerpEquivalence` | `.../test_motion_camera_slerp.cpp` | State-only | FR-021(a): `cameraRotationAt` bitwise against Reyes' sequence and the conjugate forms, 600 cases |
| `Motion_TransformRayEquivalence` | `.../test_motion_transform_ray.cpp` | State-only | FR-019/FR-021: `transformRay` against a verbatim copy of `objectMisc.h:178-232`, 5 xform kinds x 9 rays x 8 times, plus GitHub #106 pinned |
| `Frame_ParamsEqualCore` | `.../test_frame_params_equal_core.cpp` | Consumer (`ribRender`) | FR-001/FR-002/decision 5: 24 in-process sessions (4 shutters x 6 configurations), every field equals its `CRenderer` static bitwise from inside the frame; default restored after `RiWorldEnd` |

**Registration.** `tests/unit/state/CMakeLists.txt`: `cmake_dependent_option(OPENRENDER_CAMERA_MOTION_ONLY_TESTS ... OFF "OPENRENDER_STATE_ONLY_TESTS" OFF)`; with it ON the `ray.h` copy is staged beside `ri_config.h` and `frameParams.cpp`/`camera.cpp` join `state_only_sources` while they exist.
`tests/unit/camera/CMakeLists.txt` (new): `add_camera_motion_test`, the six camera/frame tests. `tests/unit/motion/CMakeLists.txt`: the four motion State-only tests (option ON) and `Frame_ParamsEqualCore` (a plain Consumer, `EXCLUDE_FROM_ALL` + `DISABLED`). All 11 are `EXCLUDE_FROM_ALL` + `DISABLED`; labels `camera;unit` and `motion;unit`.

**(a)** CP-a in `build-028` (option OFF): clean.

**(b)** Scratch tree `build-028-red`, configured like `build-028` plus `-DOPENRENDER_CAMERA_MOTION_ONLY_TESTS=ON` (deleted afterwards), `cmake --build ... --target <exe>` for each of the 10 State-only targets. Each failed to compile:

| Target | First error |
|---|---|
| `test_camera_analytic` | `tests/unit/camera/test_camera_analytic.cpp:30:10: fatal error: 'camera.h' file not found` |
| `test_camera_equivalence` | `test_camera_equivalence.cpp:34:10: fatal error: 'camera.h' file not found` |
| `test_camera_crop` | `test_camera_crop.cpp:27:10: fatal error: 'camera.h' file not found` |
| `test_camera_generate_ray_only` | `test_camera_generate_ray_only.cpp:33:10: fatal error: 'camera.h' file not found` |
| `test_camera_lens_model` | `test_camera_lens_model.cpp:25:10: fatal error: 'camera.h' file not found` |
| `test_frame_default` | `tests/unit/camera/frame_test_utils.h:31:10: fatal error: 'frameParams.h' file not found` |
| `test_motion_two_key`, `test_motion_isolation`, `test_motion_camera_slerp`, `test_motion_transform_ray` | the same `'frameParams.h' file not found` via `frame_test_utils.h`, which each includes first; `motion.h` is the next missing header once `frameParams.h` exists (the include order is not part of what the test pins) |

**(c)** `build-028`: `cmake --build build-028 --target test_frame_params_equal_core` fails: `test_frame_params_equal_core.cpp:38:10: fatal error: 'frameParams.h' file not found`.

**(d)** Disabled tests do not fail the run. (d1) `build-028`, `ctest -L '^camera$|^motion$'`: `Frame_ParamsEqualCore` "Not Run (Disabled)", the 8 replay tests pass, **exit status 0** (the 10 State-only tests do not exist there, the option being OFF). (d2) `build-028-red`, `ctest -R '^(Camera_|Frame_DefaultValue$|Motion_(TwoKeyModel|Isolation|CameraSlerpEquivalence|TransformRayEquivalence)$)'`: the 10 tests report "Not Run (Disabled)", **exit status 0**.

**Pre-validation of the tests (scratch only, nothing under `src/`).** A frozen test that turns out wrong costs a re-approval and a Linux re-baseline, so before this checkpoint the tests were run once against a throwaway implementation of the contracts that is NOT in the repository (`build-028-scratch` and the session scratchpad):
the 10 State-only tests were compiled with `-Wall -Wextra -Wpedantic -Werror` against a stub `frameParams`/`camera`/`motion` (every one PASSED), and `Frame_ParamsEqualCore` was built in a second scratch tree with a minimal `rendererFrame.cpp` (24 sessions, 744 checks, 0 failures after one test correction: with an empty shutter the two camera samples collapse, so a camera rotation is not installed; the rotation expectations are now gated on a non-empty shutter).
The same throwaway code served to try the mutation plan of Phase 3 (all red, below).

**Mutation plan pre-checked (T047 to T049 repeat these in the worktree):**

| Task | Test | Mutation | Result on the scratch implementation |
|---|---|---|---|
| T047 (1) | `Camera_Analytic` | flip the sign of the lens offset in `generateRay` | red (13 failed checks) |
| T047 (2) | `Camera_StartingTreeEquivalence` | `dir = normalizev(subvv(from, aim))` | red (7: one per configuration) |
| T047 (3) | `Camera_CropInvariance` | read `sampling.originX` instead of `screenLeft` | red (77) |
| T047 (4) | `Camera_GenerateRayOnly` | default `evaluateImportance` returns `Supported` | red (2) |
| T047 (5) | `Camera_LensModel` | `cocFactorScreen()`: `focaldistance - aperture` in the denominator | red (3) |
| T047 (6) | `Frame_DefaultValue` | default `invShutterTime` = 2 | red (3) |
| T048 (1) | `Motion_TwoKeyModel` | `atTime` operand order `key1 * t + key0 * (S(1) - t)` | red (260; the FMA contraction choice changes) |
| T048 (2) | `Motion_Isolation` | `#include "renderer.h"` in `motion.h` | red (build fails: not on the narrow path) |
| T048 (3) | `Motion_CameraSlerpEquivalence` | `slerpq(R, q, identQ, t)` argument order | red (919) |
| T048 (4) | `Motion_TransformRayEquivalence` | apply camera rotation when `xform->cameraMotion` is false (#106) | red (3) |
| T049 (1) | `Frame_ParamsEqualCore` | `invShutterTime` field copied from `shutterTime` | red (42 failed checks) |
| T049 (2) | `Frame_ParamsEqualCore` | `endFrameParams()` does not uninstall | red (44) |

**T034.** CP-a, CP-b and CP-c on the tests-only tree: build clean; 847/847 passed (the 11 new tests are Not Run or do not exist, as above), 183 s; 21/21 `architecture` tests; report equal to the baseline (`DB-004 x 65`, `DB-011 x 173`, Geometry 78/19, Hiders 567/84, TOTAL 721/110, `RESULT PASS`). `git diff --stat 4eb683b -- src` is empty.

**Linux risks to watch at the gate (T036):** GCC contracts floating-point more aggressively (`-ffp-contract=fast`) than Clang; `Camera_StartingTreeEquivalence`, `Motion_TwoKeyModel` (the polygon fused case calls `atTime<double>` with the sub-expressions inline, as the site will) and `Motion_TransformRayEquivalence` compare a verbatim copy with the library function, which only agree if both are contracted the same way. Also the explicit-instantiation idiom of `motion_replay.cpp` and `-Wall -Wextra -Wpedantic -Werror` over the new test files under GCC 13.

**T035, test approval (maintainer, 2026-10-27).** The 11 new tests, the replay tool and the shrunk replay `tests/unit/motion/replay/macos.txt` (12 943 records, 1.62 MB, sha256 `79fc6be53d5c9200d35779f0a2f3b9eb7126c08de344242d75019e414be41ef0`) are approved and **frozen**. Any later edit to them needs the maintainer's explicit approval and a redo of T036 and T037. The Linux baseline gate (T036/T037) is run by the coordinator over ssh; Phase 3 does not start until the coordinator releases the gate.

## §3 Linux baseline gate (phase 2, T036, T037)

(The handover procedure and the verification of the five Linux files.)

## §4 Unit-test mutations, greps and diffs (phase 3, T047 to T050)

(One row per mutation: file, edit, the red test, revert confirmed. The FR-003 and scope greps.)

## §5 Allowlist `VIOLATION` lines and SC-008 timing (phases 4 to 11)

(Per allowlist phase: the exact `VIOLATION` lines before the code change. The interleaved timing tables of T056 and T100.)

## §6 Final macOS pass (phase 12, T109, T110)

(CP-a to CP-d, `-L visual`, `-L shading_parity`, `-L '^determinism$'`, the sweep once.)

## §7 Linux finished tree (phase 12, T111)

(The procedure, the results against the `-linux` baselines, and the determinism sweep once on Linux.)
