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
| 3 | clean (`-Werror`) | 858/858 passed (847 + the 11 new tests, all enabled), 206 s | `DB-004 x 65`, `DB-011 x 173`; Geometry 78/19, Hiders 567/84, TOTAL 721/110 (equal to the baseline); `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; 21/21 `architecture` tests | 761 images: **0 differing, 0 missing, 0 not in the manifest**; `MaxBlockAvgDiff` table identical (empty diff) | 19/19 (11 new + 8 replay) |
| 4 | clean (`-Werror`) | 858/858 passed, 180 s | `DB-004 x 65`, **`DB-011 x 166`** (173 - 7); nothing added; Hiders 558/82 (was 567/84), TOTAL 712/108 (was 721/110), Geometry 78/19; `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; 21/21 `architecture` tests | 761 images: **0 differing, 0 missing, 0 not in the manifest**; `MaxBlockAvgDiff` table identical | 19/19 |
| 5 | clean (`-Werror`) | 858/858 passed, 209 s | **`DB-004 x 59`** (65 - 6), `DB-011 x 166`; Geometry 72/16 (was 78/19), Hiders 558/82, TOTAL 706/108; `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; 21/21 `architecture` tests | 761 images: **0 differing, 0 missing, 0 not in the manifest**; `MaxBlockAvgDiff` table identical | 19/19 |
| 6 | clean (`-Werror`) | 858/858 passed, 223 s | `DB-004 x 58` (59 - 1), `DB-011 x 166`; Geometry 58/16 (was 72/16), Hiders 558/82, TOTAL 692/108; `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; 21/21 `architecture` tests | 761 images: **0 differing, 0 missing, 0 not in the manifest**; `MaxBlockAvgDiff` table identical | 19/19 |
| 7 | clean (`-Werror`) | 858/858 passed, 190 s | `DB-004 x 57` (58 - 1), `DB-011 x 166`; Geometry 55/16 (was 58/16), Hiders 558/82, TOTAL 689/108; `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; 21/21 `architecture` tests | 761 images: **0 differing, 0 missing, 0 not in the manifest**; `MaxBlockAvgDiff` table identical | 19/19 |
| 8 | clean (`-Werror`) | 858/858 passed, 190 s | `DB-004 x 56` (57 - 1), `DB-011 x 166`; Geometry 53/16, Hiders 558/82, TOTAL 687/108; `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; 21/21 `architecture` tests | 761 images: **0 differing, 0 missing, 0 not in the manifest**; `MaxBlockAvgDiff` table identical | 19/19 |
| 9 | clean (`-Werror`) | 858/858 passed, 188 s | no allowlist change: `DB-004 x 56`, `DB-011 x 166`; Geometry 53/16, Hiders 558/82, TOTAL 687/108 (all unchanged from phase 8); `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; 21/21 `architecture` tests | 761 images: **0 differing, 0 missing, 0 not in the manifest**; `MaxBlockAvgDiff` table identical | 19/19 |
| 10 | clean (`-Werror`) | 858/858 passed, 213 s (second full run 858/858) | no allowlist change: `DB-004 x 56`, `DB-011 x 166`; Geometry 53/16, Hiders 558/82, TOTAL 687/108 (all unchanged from phase 9); `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; 21/21 `architecture` tests | first full run: 761 images, **1 differing** (a one-off, see Phase 10 below), 0 missing, 0 not in the manifest; second full run: **0 differing**; `MaxBlockAvgDiff` table identical in both | 19/19 |
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

**T036, run by the coordinator over ssh (2026-10-27).** Host Ubuntu 24.04, 2 cores, GCC 13.3, LLVM 18.1.3. The baseline was taken on the worktree as it stood at the gate: `src/` equal to master `4eb683b` (`git diff --stat 4eb683b -- src` empty), the approved phase-2 tests, `OPENRENDER_CAMERA_MOTION_ONLY_TESTS` OFF. From phase 3 on, the tree no longer builds the starting `src/` with these tests, so this baseline cannot be taken later.

- **Sync**: the 2974 tracked files (`git ls-files`) went to a fresh remote directory `/home/juvenal/Projects/Development/CLI/openrender-028` (`rsync -rlpc --files-from`, no `-t`). `sha256sum -c` of the Mac manifest (`shasum -a 256`, one line per file) reported every file OK. A SHA-256 over the per-file hashes was the same on both sides: `c2544538283b050d980b74bc18271fdc653bfea584744b7b411d72535ab24ace`.
- **Configure and build**: `cmake -B build-028 -S . -DCMAKE_BUILD_TYPE=Release -DINSTALL_SELFCONTAINED=OFF`, JIT on. `cmake --build build-028 -j2` through the remote `cpujob`: exit 0, clean under `-Werror` (0 errors, 0 warnings, the new tests and `motion_replay.cpp` included).
- **Architecture**: `ctest -L architecture` 21/21. The report ([tools/archcheck-base-linux.txt](tools/archcheck-base-linux.txt)) is the `Architecture_DomainBoundaries` output from ctest's `LastTest.log`:

  ```text
  archcheck: platform=linux objects=275 sources=188 edges=2424 (include=1511 link=913)
  ALLOWED   DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-010 x 35 DB-011 x 173 DB-015 x 10
            DB-017 x 78 DB-019 x 3 DB-021 x 1 DB-025 x 12 DB-026 x 3
  METRIC    Geometry 78/19, Hiders 567/84, Infra 0/0, SceneInterface 6/6, Shading 0/0,
            Texturing 70/16, TOTAL 721/110 (all equal to metrics-baseline.txt)
  RESULT    PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0
  ```

  This is the Linux baseline for T111: `DB-004 x 65`, `DB-011 x 173` (175 lines minus the two macOS-tagged ones, as predicted in §0.2). The `ALLOWED` and `METRIC` rows are identical to macOS. The only platform difference is the header's object, source and include-edge counts (macOS 270, 185, 1506).
- **Replay**: `ctest -R '^Motion_ReplayRecord$'` was run twice. `cmp` of the two `build-028/tests/unit/motion/replay-recorded-linux.txt` outputs: **byte-identical**. The first was copied unedited to `tests/unit/motion/replay/linux.txt`: 12 944 lines, header `# motion_replay v1 linux master-4eb683b` plus 12 943 records, the same count as macOS. Records per family: quadrics 4270, raytransform 2888, gridonly 2511, tessellation 1525, polygons 610, emptyshutter 610, bilinear 305, implicit 224. 3963 records differ from `macos.txt` (floating-point differences between the two compilers, as the T036 risk note expected), which is why each platform keeps its own file. Then `ctest -R '^Motion_Replay_'`: **8/8 passed**.
- **Full suite** `ctest -LE determinism-sweep -j2`, twice, with the image stamp before each run and `LastTest.log` copied right after it:

  | Run | Result | Real time |
  |---|---|---|
  | 1 | 841 of 848 passed, 7 failed; `Frame_ParamsEqualCore` Not Run (Disabled), `LibShader_Format_rslo_lower_minor` skipped by the test itself | 639 s |
  | 2 | the same 7 failed, the same Not Run and skip | 631 s |

  All 7 failures are Linux failures that predate spec 028, with the signatures already recorded in specs 022 to 024 and 039:
  - `LibShader_McompConsistency`: #44, `symbol lookup error` in `libshader_shading.so.1`.
  - `TextureTile_RenderByteIdentical`: #45.
  - `TextureTile_CacheByteIdentical`, `TextureTile_CacheDefaultDirectory`, `TextureTile_CacheStaleDetection` and `TextureTile_CacheConcurrentWrite`: #46, `orender` aborts with a core dump.
  - `CoverageAnalytic_alpha-sides2-tjunction-r4_reyes`: #57, one pixel off, (99, 46) at 0.875 against 0.9375.

  `Visual_spotlight-raytrace` (#105) passed in both runs. There is no failure outside the known Linux set.
- **Image manifest**: run 1 recorded **761 images** ([tools/baseline-images-linux.sha256](tools/baseline-images-linux.sha256)), the same 761 paths as macOS. 279 of them have the same hash on both platforms.
- **`MaxBlockAvgDiff` table**: 389 tests ([tools/baseline-diffs-linux.txt](tools/baseline-diffs-linux.txt)), **identical in both runs**. The tests are the same as macOS, and 27 values differ from macOS. A tool artifact: Ubuntu's default `awk` is `mawk`, under which `image_manifest.sh diffs` puts a leading comma before every value (`Parity_aov	,7.25`). It is consistent from run to run on Linux, so CP-d at T111 is valid as long as it runs on the same host with the same `awk`. The file was not edited, and neither was the tool.
- **Run-to-run noise**: `image_manifest.sh diff` of run 2 against run 1: `manifest 761 images, fresh 761, noise-listed 0; 1 differing, 0 missing, 0 not in manifest`. The one image that differs is `tests/shading_parity/test.tif`, the shared scratch output of `test_matrix_transform_jit`, `test_printf_jit` and `test_uniform_conditional_jit` (the macOS §0.3 race). It is the only entry in [tools/baseline-image-noise-linux.txt](tools/baseline-image-noise-linux.txt). `tests/shading_parity/ri.tif`, which is on the macOS list for the same reason, was identical in both Linux runs.
- **Files copied back**: their checksums were checked against the remote copies with `sha256sum -c`, and all were OK:

  | File | sha256 |
  |---|---|
  | `tools/archcheck-base-linux.txt` | `9e32a961a05463a41f46f47ab9425ecf6041e015002e1df13db466e2211aded2` |
  | `tools/baseline-images-linux.sha256` | `dd43c76b2ad24a692554b026d0480a2f029b84466889382c3dac428f4cd3dd62` |
  | `tools/baseline-diffs-linux.txt` | `a507789e7c28ab9a3c0a5af880d3083db4bc54bcb27a00e2b20f05d71688cb4c` |
  | `tools/baseline-image-noise-linux.txt` | `c9661e13c01fc13ac33966c549f60b46e49a54b9d545405ac19da8d267a0d155` |
  | `tests/unit/motion/replay/linux.txt` | `94fc53eec5dec4cf05cf37673375803c9fba25f04a3834d1516034b1c10efa48` |

  The remote tree and its logs (`build-028/logs/`) stay in place, at 204 MB.

**T037 check.**
- The five files exist and are non-empty. The replay header reads `# motion_replay v1 linux master-4eb683b`.
- `archcheck-base-linux.txt` shows `RESULT PASS`, with no platform-tagged difference in `ALLOWED` or `METRIC` from macOS.
- The coordinator's logs show the eight `Motion_Replay_*` tests green, the architecture check PASS and two suite runs.
- `git diff --stat 4eb683b -- src` is still empty.
- `linux.txt` is `Motion_ReplayRecord`'s output, unedited: `cmp` against the recorded run-1 file reports no difference.

**The gate's criteria are met. Phase 3 waits for the coordinator to release the gate.**

## §4 Unit-test mutations, greps and diffs (phase 3, T047 to T050)

**Checkpoint 3 approved by the maintainer (2026-10-28).**

**Mutations** (T047 to T049): each is a scratch edit of a staged file, a rebuild of the test's target, the named test red, then `git checkout -- <file>` and `git diff -- <file>` empty (checked for every one). Run again with a pause of more than a second between each edit and each revert (see the note below).

| Task | Test | Mutation | Result |
|---|---|---|---|
| T047 (1) | `Camera_Analytic` | `camera.cpp`: `from[COMP_X] -= lensUV[0] * aperture` (lens offset sign flipped) | red, 12 failed checks |
| T047 (2) | `Camera_StartingTreeEquivalence` | `camera.cpp`: `subvv(ray.dir, from, to)` in the pinhole branch | red, 6 |
| T047 (3) | `Camera_CropInvariance` | `camera.cpp`: `screenLeft((float)fp.sampling.originX)` | red, 76 |
| T047 (4) | `Camera_GenerateRayOnly` | `camera.cpp`: default `evaluateImportance` returns `Supported` | red, 1 |
| T047 (5) | `Camera_LensModel` | `camera.cpp`: `focaldistance - aperture` in `cocFactorScreen()` | red, 2 |
| T047 (6) | `Frame_DefaultValue` | `frameParams.cpp`: default `invShutterTime = 2` | red, 2 |
| T048 (1) | `Motion_TwoKeyModel` | `motion.h`: `atTime` as `key1 * t + key0 * (S(1) - t)` | red, 259 |
| T048 (2) | `Motion_Isolation` | `motion.h`: `#include "renderer.h"` | red: the target does not build (`'renderer.h' file not found` on the narrow path) |
| T048 (3) | `Motion_CameraSlerpEquivalence` | `motion.h`: `slerpq(Rjt, q, identQ, t)` | red, 918 |
| T048 (4) | `Motion_TransformRayEquivalence` | `motion.h`: camera rotation applied without `xform->cameraMotion` (GitHub #106 case) | red, 2 |
| T049 (1) | `Frame_ParamsEqualCore` | `rendererFrame.cpp`: `invShutterTime` copied from `shutterTime` | red, 42 |
| T049 (2) | `Frame_ParamsEqualCore` | `rendererFrame.cpp`: `endFrameParams()` does not uninstall | red, 44 |

**A build hazard found on the way.** macOS's `/usr/bin/make` (GNU make 3.81) compares whole-second modification times, so an edit made in the same second as the previous build of that file is not seen. The first mutation run therefore reported `T047-3` as "STILL GREEN" (a stale object), and the CP-b run that followed it saw three camera tests fail against stale mutated objects (`Camera_Analytic`, `Camera_StartingTreeEquivalence`, `Camera_CropInvariance`).
Both were artifacts of the mutation script, not of the code: the script now waits 1.2 s after every apply and revert and touches the file, all 12 mutations were re-run and are red, and CP-a to CP-d were repeated on a tree rebuilt after the last revert. **Any future script that edits and rebuilds in quick succession must do the same.**

**T050 greps and diffs:**
- `grep -nE '#include *"(.*/)?(renderer|rendererContext)\.h"' src/ri/state/frameParams.* src/ri/state/camera.* src/ri/geometry/motion.h` prints nothing (FR-003).
- `git diff --stat 4eb683b --` the "files 028 never edits" list (texture/, `rendererServicesImpl.h`, `rendererServices.h`, `shading.{h,cpp}`, `renderer.h`, `rendererStatics.cpp`, `rendererContext.cpp`, `reyes.{cpp,h}`, `stochastic.{cpp,h}`, `stochasticQuad.h`, `zbuffer.{cpp,h}`, `sampler.{h,cpp}`, `object.cpp`, `csgDefinition.cpp`, `implicit.h`, `dlo.h`) is empty; so is `surface.cpp`.
- The Reyes and ray SLERP call shapes `Motion_CameraSlerpEquivalence` copies are all still there: `grep -nE 'slerpq\(Rjt(_pix)?, identQ, ' ...` lists eight sites (`reyes.cpp` 1433, 1604, 1643; `stochasticQuad.h` 520, 884; `rendererContext.cpp` 740; `stochastic.cpp` 220, conjugate `invRotQ`; `objectMisc.h` 213, conjugate `relRotQ_inv`).
- `git diff --stat 4eb683b -- src`: 9 files, 769 insertions, no deletion: `CMakeLists.txt` (+4), `renderer.cpp` (+8), and the new `frameParams.{h,cpp}`, `camera.{h,cpp}`, `motion.h`, `rendererFrame.{h,cpp}`.
- Files naming the qualified `CRenderer::invShutterTime` (SC-001's count, 9 before): the 7 Geometry files, `rendererServicesImpl.h`, `rendererStatics.cpp` and now `rendererFrame.cpp`: 10 until phase 11 deletes the Geometry ones (the target is 3).
- `archcheck` lists `src/ri/geometry/motion.h` as `UNRESOLVED` (no compiled translation unit of the production build includes it yet; no edges derived). That is expected until phase 5 includes it from `dlobject.cpp` and `quadrics.cpp`; the result is `PASS`.

## §5 Allowlist `VIOLATION` lines and SC-008 timing (phases 4 to 11)

### Phase 4 (camera, T052 to T057)

**Checkpoint 4 approved by the maintainer (2026-10-28).**

**T052.** The 7 DB-011 lines of `src/ri/hiders/raytracer.cpp` (`imagePlane`, `projection`, `focaldistance`, `invImagePlane`, `aperture`, `dxdPixel`, `dydPixel`) were deleted from `tests/architecture/allowlist.txt` (7 deletions, 0 additions), and the architecture check, run on the unchanged code, reported exactly these 7 and nothing else (`ALLOWED ... DB-011 x 166`, `RESULT FAIL violations=7 stale=0`; `failing-cycles=1` is reported alongside the 7 edges and is gone with them):

```text
  VIOLATION link    src/ri/hiders/raytracer.cpp -> _ZN9CRenderer10imagePlaneE (CRenderer::imagePlane)  [Hiders -> Core]
  VIOLATION link    src/ri/hiders/raytracer.cpp -> _ZN9CRenderer10projectionE (CRenderer::projection)  [Hiders -> Core]
  VIOLATION link    src/ri/hiders/raytracer.cpp -> _ZN9CRenderer13focaldistanceE (CRenderer::focaldistance)  [Hiders -> Core]
  VIOLATION link    src/ri/hiders/raytracer.cpp -> _ZN9CRenderer13invImagePlaneE (CRenderer::invImagePlane)  [Hiders -> Core]
  VIOLATION link    src/ri/hiders/raytracer.cpp -> _ZN9CRenderer8apertureE (CRenderer::aperture)  [Hiders -> Core]
  VIOLATION link    src/ri/hiders/raytracer.cpp -> _ZN9CRenderer8dxdPixelE (CRenderer::dxdPixel)  [Hiders -> Core]
  VIOLATION link    src/ri/hiders/raytracer.cpp -> _ZN9CRenderer8dydPixelE (CRenderer::dydPixel)  [Hiders -> Core]
```

**T053, T054.** `CRaytracer(int thread, const ICamera &camera)` holds the reference; `computeSamples` calls `camera.generateRay(cRay->xAbs, cRay->yAbs, cRay->time, lens)` per ray with the lens point `sample()` drew, copies `from`, `dir`, `da`, `db` into the `CPrimaryRay` and sets `t`, `flags`, `tmin`, `depthProbe` as before; the camera-motion comment moved with the behaviour it describes and now names `motion::transformRay`. `renderer.cpp` constructs `new CRaytracer(i, frameCamera())`, the only construction site (`grep -rn 'CRaytracer('` finds no other, in the Python/Lua bindings and the tests included).

**T055.** CP-a clean; CP-b 858/858; CP-c: the 7 deleted lines are gone with no `STALE` and no `VIOLATION`, `DB-004 x 65`, `DB-011 x 166`; CP-d: 761 images byte-identical (the camera is on every raytrace ray, so the whole suite is the guard) and the `MaxBlockAvgDiff` table identical; the 8 replay tests and the 11 new tests green (19/19).
`grep -nE 'CRenderer::(imagePlane|invImagePlane|projection|focaldistance|aperture|dxdPixel|dydPixel)|fullImagePixels2camera' src/ri/hiders/raytracer.cpp` prints nothing.

**T056, SC-008 timing, interleaved** (five alternating pairs per scene, a starting-tree run in `build-028-scratch/bld` then a current-tree run in `build-028`, each `ctest -R '^<test>$'` alone through `cpujob`; every run in `build-028-logs/028-p4-timing.txt`; seconds):

| Test | Starting tree | Current tree | Median start | Median current | Start spread (max - min) | Delta of medians |
|---|---|---|---|---|---|---|
| `Visual_teapot-motion-raytrace` | 19.97, 19.57, 21.15, 19.57, 19.51 | 19.70, 19.92, 20.34, 19.96, 19.73 | 19.57 | 19.92 | 19.51-21.15 (1.64) | +0.35 |
| `Convergence_dof-raytrace` | 6.68, 6.83, 6.83, 6.65, 6.60 | 6.55, 6.87, 6.79, 6.73, 6.54 | 6.68 | 6.73 | 6.60-6.83 (0.23) | +0.05 |
| `Visual_blobby-motion-grow-raytrace` | 2.35, 2.62, 2.38, 2.36, 2.38 | 2.38, 2.93, 2.37, 2.39, 2.36 | 2.38 | 2.38 | 2.35-2.62 (0.27) | +0.00 |

The current tree's median does not exceed the starting tree's by more than the starting tree's own spread in any of the three scenes (the largest delta, +0.35 s on the teapot, is under a quarter of its 1.64 s spread). **SC-008 holds at phase 4.**

### Phase 5 (ray transform, T058 to T061)

**Checkpoint 5 approved by the maintainer (2026-10-28).**

**T058.** The 6 DB-004 lines (`cameraHasRotation`, `relRotQ`, `toWorld` for `dlobject.cpp` and for `quadrics.cpp`) were deleted from `tests/architecture/allowlist.txt` (6 deletions, 0 additions); the architecture check, run on the unchanged code, reported exactly these 6 and nothing else (`DB-004 x 59`, `RESULT FAIL violations=6`, `failing-cycles=1`, which clears with the fix):

```text
  VIOLATION link    src/ri/geometry/dlobject.cpp -> _ZN9CRenderer17cameraHasRotationE (CRenderer::cameraHasRotation)  [Geometry -> Core]
  VIOLATION link    src/ri/geometry/dlobject.cpp -> _ZN9CRenderer7relRotQE (CRenderer::relRotQ)  [Geometry -> Core]
  VIOLATION link    src/ri/geometry/dlobject.cpp -> _ZN9CRenderer7toWorldE (CRenderer::toWorld)  [Geometry -> Core]
  VIOLATION link    src/ri/geometry/quadrics.cpp -> _ZN9CRenderer17cameraHasRotationE (CRenderer::cameraHasRotation)  [Geometry -> Core]
  VIOLATION link    src/ri/geometry/quadrics.cpp -> _ZN9CRenderer7relRotQE (CRenderer::relRotQ)  [Geometry -> Core]
  VIOLATION link    src/ri/geometry/quadrics.cpp -> _ZN9CRenderer7toWorldE (CRenderer::toWorld)  [Geometry -> Core]
```

**T059.** The 8 ray `transform(oFrom, oDir, xform, <ray>)` calls (`dlobject.cpp:99` and the seven quadrics in `quadrics.cpp`, at `:197, 687, 1057, 1500, 1956, 2402, 2957`) are `openrender::motion::transformRay(...)` and both files include `motion.h`; `objectMisc.h`'s ray `transform()` is deleted (the grid `transform/vtransform/ntransform` overloads with a time argument stay for phase 6).

**T060.** CP-a clean; CP-b 858/858; CP-c: the 6 lines are gone with no `STALE` and no `VIOLATION`, `DB-004 x 59`, `DB-011 x 166`, and `src/ri/geometry/motion.h` is no longer `UNRESOLVED`; CP-d: 761 images byte-identical and the `MaxBlockAvgDiff` table identical (the quadrics scenes under object transform motion and the camera-motion scenes are in the suite); `Motion_Replay_*` 8/8 and the 11 new tests green (19/19).
`grep -n 'cameraHasRotation|relRotQ|toWorld' src/ri/geometry/*.h src/ri/geometry/*.cpp` finds only `motion.h` (reading `frameParams().camera`) and unrelated parameter names (`toWorld` in `blobbyRepeller.{h,cpp}` and `blobbyField.{h,cpp}`, the primitive's own local-to-world matrix): none is one of the 6 edges.

### Phase 6 (quadrics, T062 to T073)

**Checkpoint 6 approved by the maintainer (2026-10-28).**

**Checkpoint 5 approved by the maintainer (2026-10-28).**

**T062.** The 1 DB-004 line `src/ri/geometry/quadrics.cpp -> _ZN9CRenderer14invShutterTimeE` was deleted (1 deletion, 0 additions); on the unchanged code the architecture check reported exactly one `VIOLATION`:

```text
  VIOLATION link    src/ri/geometry/quadrics.cpp -> _ZN9CRenderer14invShutterTimeE (CRenderer::invShutterTime)  [Geometry -> Core]
```

(`DB-004 x 58`, `RESULT FAIL violations=1`, `failing-cycles=1`, which clears with the fix.)

**T063 to T070, the code** (`quadrics.cpp`, every site keeps its own expression shape and precision, so no output changes):
- the `transformPoints()` macro calls `motion::transformAtTime`, `vtransformAtTime` and `ntransformAtTime` (moved verbatim into `motion.h` in phase 3);
- the 24 intersect-side parameter lerps `(float)(this->p * (1.0 - rv->time) + nextData[k] * (double)rv->time)` (the disk, cone, paraboloid, cylinder, hyperboloid and toroid ones wrote `(1.0 - (double)rv->time)`, the same value) became `motion::atTimeD(this->p, nextData[k], rv->time)`, and the hyperboloid's two `interpolatev(p, this->p, &nextData[k], rv->time)` became `motion::atTimeV`;
- the 7 sample-side lerps of the sphere and the disk, written with `const double ctime` initialised from a float array (`*time++`), became `motion::atTimeD(this->p, nextData[k], (float)ctime)`: the narrowing is lossless because `ctime` always holds a float value, and the members and `nextData` are floats;
- the 14 dPdtime scales `mulvf(dest|D, CRenderer::invShutterTime)` became `motion::toShutterRate(dest|D)`.
The macro's backslashes were re-aligned to the longer lines (the file was not clang-format clean before, so no other formatting was touched).

**T071.** The three grid `transform/vtransform/ntransform` overloads with a time argument are deleted from `objectMisc.h` (no caller was left: the build is the proof); the dead `interpolate()`/`interpolate3()` helpers stay for phase 11.

**T072.** CP-a clean; CP-b 858/858; CP-c: the line is gone with no `STALE` and no `VIOLATION`, `DB-004 x 58`, `DB-011 x 166`; CP-d: 761 images byte-identical and the `MaxBlockAvgDiff` table identical; `Motion_Replay_quadrics` and the other 18 `motion`/`camera` tests green (19/19). `grep -c 'CRenderer::invShutterTime' src/ri/geometry/quadrics.cpp` prints 0.

### Phase 7 (patches, T074 to T079)

**Checkpoint 7 approved by the maintainer (2026-10-28).**

**Checkpoint 6 approved by the maintainer (2026-10-28).**

**T074.** The 1 DB-004 line `src/ri/geometry/patches.cpp -> _ZN9CRenderer14invShutterTimeE` was deleted (1 deletion, 0 additions); on the unchanged code the architecture check reported exactly one `VIOLATION`:

```text
  VIOLATION link    src/ri/geometry/patches.cpp -> _ZN9CRenderer14invShutterTimeE (CRenderer::invShutterTime)  [Geometry -> Core]
```

(`DB-004 x 57`, `RESULT FAIL violations=1`, `failing-cycles=1`, which clears with the fix.)

**T075 to T077, the code** (`patches.cpp`):
- the four `interpolatev(tN, PNN, PNN + vertexSize * 4, cRay->time)` of `CBilinearPatch::intersect` became `motion::atTimeV` (same key addresses, same order);
- the sample-side key lerps of the bilinear patch (`:372`) and of the bicubic patch (`:692`), whose keys are `const float *`, became `motion::atTimeD(vertex0[j], vertex1[j], (float)ctime)` (`ctime` is a `const double` filled from a float array, so the narrowing is lossless);
- the three dPdtime scales (`:481`, `:793`, and the one inside the NURBS macro at `:1311`) became `motion::toShutterRate(dest)`;
- GitHub #108 is preserved bit for bit: the `CBilinearPatch::sample` dPdtime expression is untouched, with a two-line comment citing #108 above it.

**A finding the replay caught, and its fix (before the checkpoint).** The first migration of the NURBS patch's sample-side lerp (`:1022`) used `atTimeD` like the other two. `Motion_Replay_tessellation` went red in 17 `nurbs` `S` records (the images and the `MaxBlockAvgDiff` table were unchanged, the replay being the finer guard): that site's keys are `const double *vertex0/vertex1` and its target is a `double *`, so `atTimeD`'s `float` parameters rounded the keys. The site now calls the template at its own precision, `(float)motion::atTime<double>(vertex0[j], vertex1[j], ctime)`, which is the starting tree's expression exactly (the same one definition of the model, as the polygons' fused sites use); the replay is green again. **Nothing was re-recorded and no test or reference was touched.** Lesson for the remaining sites (phase 11, `patchgrid.cpp` and `bsplinePatchgrid.cpp`): check the key type of every lerp before choosing `atTimeD`.

**T078.** CP-a clean; CP-b 858/858; CP-c: the line is gone with no `STALE` and no `VIOLATION`, `DB-004 x 57`, `DB-011 x 166`; CP-d: 761 images byte-identical and the `MaxBlockAvgDiff` table identical; `Motion_Replay_bilinear` and `_tessellation` (its `S` records of the bicubic and NURBS scenes pin the three sample paths and the #108 expression) green with the 17 other `motion`/`camera` tests (19/19). `grep -c 'CRenderer::invShutterTime' src/ri/geometry/patches.cpp` prints 0.

### Phase 8 (polygons, T080 to T085)

**Checkpoint 8 approved by the maintainer (2026-10-28).**

**Checkpoint 7 approved by the maintainer (2026-10-28).**

**Phase 7 metrics re-measured on the staged code** (the first job of this phase, before any edit): `ALLOWED DB-004 x 57, DB-011 x 166`; `METRIC Geometry 55/16, Hiders 558/82, TOTAL 689/108`; `RESULT PASS`. The figures in the phase 7 row are those of the staged code.

**T080.** The 1 DB-004 line `src/ri/geometry/polygons.cpp -> _ZN9CRenderer14invShutterTimeE` was deleted (1 deletion, 0 additions); on the unchanged code the architecture check reported exactly one `VIOLATION`:

```text
  VIOLATION link    src/ri/geometry/polygons.cpp -> _ZN9CRenderer14invShutterTimeE (CRenderer::invShutterTime)  [Geometry -> Core]
```

(`DB-004 x 56`, `RESULT FAIL violations=1`, `failing-cycles=1`, which clears with the fix.)

**T081 to T083, the code** (`polygons.cpp`; key types checked first: every key the polygons read is a `const float *`, and `ctime`, `cu`, `cv` are doubles):
- the triangle's three and the quad's four intersect `interpolatev(tN, ..., vertices + this->vN * 3, cRay->time)` became `motion::atTimeV` (same key addresses, same order), and so did the normal's `interpolatev(dest, normal0, normal1, time[i])` in `sample()`;
- the 14 fused sample statements (the triangle's and the quad's `P` and `dPdu`/`dPdv` temporal interpolation, each one double expression) became `(float)motion::atTime<double>(S0, S1, ctime)`, where `S0` and `S1` are the site's own two spatial double sub-expressions, written exactly as before and passed inline (nothing hoisted, no statement split, no operand reordered: per-expression contraction is the same, as `Motion_TwoKeyModel`'s polygon case pins);
- the two dPdtime scales (`mulvf(dest - 3, ...)` and `mulvf(dest, ...)`) became `motion::toShutterRate`.
The first attempt at the substitution used a pattern that could run across statements and rewrote two unrelated lines; it was caught in the diff before any build and redone with a pattern bounded to one statement.

**T084.** CP-a clean; CP-b 858/858; CP-c: the line is gone with no `STALE` and no `VIOLATION`, `DB-004 x 56`, `DB-011 x 166`; CP-d: 761 images byte-identical and the `MaxBlockAvgDiff` table identical; `Motion_Replay_polygons` and the other 18 `motion`/`camera` tests green (19/19). `grep -c 'CRenderer::invShutterTime' src/ri/geometry/polygons.cpp` prints 0.

### Phase 9 (implicit surface, T086 to T088)

**Checkpoint 9 approved by the maintainer (2026-10-28).**

**Checkpoint 8 approved by the maintainer (2026-10-28).**

**T086.** The three plug-in time reads of `CImplicit::intersect` (`implicitSurface.cpp:142, 155, 176`: the two `evalFunction` calls of the ray marcher and the `evalNormalFunction` call) pass `openrender::motion::pluginTime(*ray)`, the ray's own unclamped shutter fraction, instead of `ray->time`; `motion.h` is included. No arithmetic moved. **`implicit.h` (the Procedures ABI) is unchanged** (`git diff 4eb683b` on it, and on `dlo.h`, is empty), so DB-003 is untouched. There is no allowlist line to delete for this phase (research R11).

**T087.** CP-a clean; CP-b 858/858; CP-c: no allowlist change, `DB-004 x 56`, `DB-011 x 166`, `RESULT PASS`; CP-d: 761 images byte-identical and the `MaxBlockAvgDiff` table identical; `Motion_Replay_implicit` (the first coverage of `CImplicit`) and the other 18 `motion`/`camera` tests green (19/19).

### Phase 10 (tessellation, T089 to T091)

**T089.** In `CTesselationPatch::intersect`'s moving branch of `src/ri/geometry/surface.cpp`, the 20 `interpolatev(P.., Pt0/Pt1 or cP0/cP1, timev)` calls (div 1x1 and the four `cP0`/`cP1` branches; the key pointers are `const float *`) became `openrender::motion::atTimeV`. `#include "motion.h"` was added at `:38`. `git diff 4eb683b -- src/ri/geometry/surface.cpp | grep '^@@'` shows six hunks: the include, and five (`:1458`, `:1487`, `:1509`, `:1551`, `:1594`) inside `CTesselationPatch::intersect`. The dicing-probe `timev` uses and the macro `interpolatev(dPdu/dPdv)` at `:1127/1130` are unchanged. No allowlist line applies to this phase.

**T090.** CP-a clean; CP-b 858/858; CP-c: no allowlist change, `DB-004 x 56`, `DB-011 x 166`, `RESULT PASS`, 21/21 `architecture` tests; CP-m 19/19 (`Motion_Replay_tessellation` green, no replay difference); `MaxBlockAvgDiff` table identical.

**CP-d one-off (investigated at the maintainer's request, 2026-10-28).** The first full run had one differing image of the 761: `build-028/tests/visual/coverage-alpha-sides2-tjunction-r0.5_reyes/coverage-alpha-sides2-tjunction-r0.5-reyes.tif` (written by `CoverageAnalytic_alpha-sides2-tjunction-r0.5_reyes`, test #218, which passed with the same `MaxBlockAvgDiff`). Hashes: differing `a5d945b606ce4337f7128ab2a1de0102f54edce158d145024ef1ba2d70926c9e`, baseline `65e786c887f3a5413e0ab077ed859170b6ea02b04bfce759cc5b35e96fc1589f`.
- *Pixel analysis* (both files decompressed with `tiffcp -c none` and compared; `scratchpad/tjcmp.py`): the image is a 160x120 single-channel float (alpha). **Exactly 1 of 19200 pixels differs**: x=119, y=64; the differing run holds 0.0 where the baseline holds 0.75 (the two-layer alpha of one sphere with `Sides 2`, `Opacity 0.5`). Value counts: baseline 0.0 x 12855, 0.75 x 5114, 0.9375 x 1231; differing run 0.0 x 12856, 0.75 x 5113, 0.9375 x 1231.
- *Geometry under that pixel*: working the ray through the pixel centre (fov 45, 160x120, camera z = 6) gives a distance from the centre of the small sphere (centre `0.5 0.3 3` in camera space, radius 0.5) to the ray of about 0.497 against 0.5, so the pixel centre lies about 0.6% inside that sphere's silhouette: a silhouette-edge pixel of the small sphere, one pixel dropped out. It also sits on row 64, which would be the top row of a bucket row if the bucket height is 16. The scene is static REYES (`Hider "reyes" "jitter" [0]`, `PixelSamples 1 1`, no `MotionBegin`) and cannot reach the raytracer moving branch this phase edits.
- *Variance measurement* (23 repeats of the test after the first occurrence, within the 40-repeat limit): 2 isolated reruns, then 20 more isolated runs through `cpujob`/ctest, all `rc=0` and all `65e786c887f3a541...` (the baseline hash; `build-028-logs/028-p10-iso-hashes.txt`); then one more full `-j4` suite (858/858, 761 images **0 differing**, `MaxBlockAvgDiff` identical, this image back at `65e786c887f3a541...`). The difference **did not recur in 23 repeats**; it is recorded as a one-off, not as a noise-list entry (none added; no reference touched; no issue filed).
- *Evidence files* in `build-028-logs/`: `p10-tj-run0.tif` (the differing image), `p10-cov-run1.tif` and `p10-cov-run2.tif` (baseline-matching), `u-run0.tif` and `u-base.tif` (decompressed copies), `028-p10-ctest.log`, `028-p10-ctest2.log`, `028-p10-LastTest.log`, `028-p10-LastTest2.log`, `028-p10-iso-hashes.txt`.
- *Family*: whether this matches the T-junction coverage tests that fail on Linux (#57) is for the maintainer to judge; the scene is one of that T-junction family and the symptom is a one-pixel coverage drop on a silhouette seam, but it passed on macOS in 23 of 24 runs.

## §6 Final macOS pass (phase 12, T109, T110)

(CP-a to CP-d, `-L visual`, `-L shading_parity`, `-L '^determinism$'`, the sweep once.)

## §7 Linux finished tree (phase 12, T111)

(The procedure, the results against the `-linux` baselines, and the determinism sweep once on Linux.)
