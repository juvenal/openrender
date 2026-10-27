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

DB-011's allowlist header says `(182)`: 173 macOS lines plus the platform-tagged Linux lines. The
registry text's "180 edges per platform" is stale (FR-023).

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

(Filled in by the implement stage, one section per phase, with CP-a to CP-d.)
