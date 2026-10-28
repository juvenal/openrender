# Quickstart / Validation: Camera and Motion Layer

**Plan**: [plan.md](plan.md). Contracts: [contracts/](contracts/). Data model:
[data-model.md](data-model.md). Measured evidence: [validation.md](validation.md).

All commands run in the spec worktree:

```bash
W=/Volumes/Projects/Development/CLI/openrender-worktrees/028-camera-and-motion-layer
T=$W/specs/028-camera-and-motion-layer/tools
CJ=~/.claude/bin/cpujob
```

Every build, test and render goes through `cpujob` (one job at a time machine-wide, `nice -n 19`,
at most `-j4`; `-j2` for the determinism sweep); long jobs run in the background with a log. Never
run a test binary directly. Spec 027's agent shares the lock.

## §0 Baselines on the starting tree (master `4eb683b`), macOS — DONE at the plan stage, 2026-10-27

Results in [validation.md](validation.md) §0; raw data in [tools/](tools/).

```bash
$CJ 028-base-build cmake --build $W/build-028 --config Release -j4
$CJ 028-base-arch  ctest --test-dir $W/build-028 -L architecture --output-on-failure
$CJ 028-base-archreport $W/build-028/tests/architecture/archcheck --source-dir $W --build-dir $W/build-028 --config $W/tests/architecture
$T/image_manifest.sh stamp $W/build-028
$CJ 028-base-suite ctest --test-dir $W/build-028 -LE determinism-sweep -j4 --output-on-failure
cp $W/build-028/Testing/Temporary/LastTest.log $W/build-028-logs/base-suite1.LastTest.log
$T/image_manifest.sh record $W/build-028 > $T/baseline-images-macos.sha256
$T/image_manifest.sh diffs $W/build-028-logs/base-suite1.LastTest.log > $T/baseline-diffs-macos.txt
# noise: stamp, the suite again, then
$T/image_manifest.sh diff $T/baseline-images-macos.sha256 $W/build-028 $T/baseline-image-noise-macos.txt
```

Timing (SC-008): each of `Visual_teapot-motion-raytrace`, `Convergence_dof-raytrace` and
`Visual_blobby-motion-grow-raytrace` five times alone:
`$CJ 028-base-time ctest --test-dir $W/build-028 -R '^<test>$'`.

**Still to record in phase 1 (macOS)**, on the unchanged `src/`:
- the replay file: build `motion_replay` and its fixtures (test-only code), then record through
  ctest (never by running the binary directly):
  `$CJ 028-p1-replay ctest --test-dir $W/build-028 -R '^Motion_ReplayRecord$' --output-on-failure`.
  `Motion_ReplayRecord` (label `motion-record`, with a `TIMEOUT`) writes only into the build tree,
  `build-028/tests/unit/motion/replay-recorded-macos.txt`, so running it on a later tree is harmless.
  On the starting tree only, copy that file to `$W/tests/unit/motion/replay/macos.txt`; then
  `$CJ 028-p1-motion ctest --test-dir $W/build-028 -R '^Motion_Replay_' --output-on-failure`
  (green by construction);
- the replay mutations (contracts/replay-format.md, "Red evidence"), each in the scratch copy
  `$W/build-028-scratch` (gitignored, inside the worktree), each turning exactly its family's test red.

## §1 Red evidence (phase 2, tests written before the code)

| Test | Expected red on the starting tree |
|---|---|
| `Camera_Analytic`, `Camera_StartingTreeEquivalence`, `Camera_CropInvariance`, `Camera_GenerateRayOnly`, `Camera_LensModel`, `Frame_DefaultValue` | build failure: `camera.h`/`frameParams.h` not found. Taken in a scratch build directory configured with `-DOPENRENDER_CAMERA_MOTION_ONLY_TESTS=ON` (off in `build-028` until phase 3) |
| `Motion_TwoKeyModel`, `Motion_Isolation`, `Motion_CameraSlerpEquivalence`, `Motion_TransformRayEquivalence` | build failure: `motion.h` not found (same scratch build) |
| `Frame_ParamsEqualCore` | build failure: `frameParams.h` not found (`EXCLUDE_FROM_ALL`, `DISABLED` in `build-028` until phase 3) |
| `Motion_Replay_*` | green by construction; red only under its mutation (§0) |
| `ctest -L architecture` after a phase deletes its allowlist lines | exactly that phase's lines as `VIOLATION` (contracts/allowlist-end-state.md), until the phase's code lands |

## §2 Per-phase checkpoint (every phase from 3 on), macOS

```bash
# CP-a build (clean under -Werror)
$CJ 028-pN-build cmake --build $W/build-028 --config Release -j4
# CP-b suite, with the image stamp
$T/image_manifest.sh stamp $W/build-028
$CJ 028-pN-suite ctest --test-dir $W/build-028 -LE determinism-sweep -j4 --output-on-failure
cp $W/build-028/Testing/Temporary/LastTest.log $W/build-028-logs/pN.LastTest.log
# CP-d byte-identical output (before any other ctest run)
$T/image_manifest.sh diff $T/baseline-images-macos.sha256 $W/build-028 $T/baseline-image-noise-macos.txt
$T/image_manifest.sh diffs $W/build-028-logs/pN.LastTest.log | diff - $T/baseline-diffs-macos.txt
# CP-c architecture
$CJ 028-pN-arch ctest --test-dir $W/build-028 -L architecture --output-on-failure
$CJ 028-pN-archreport $W/build-028/tests/architecture/archcheck --source-dir $W --build-dir $W/build-028 --config $W/tests/architecture
```

Expected at every phase:
- **CP-a**: clean.
- **CP-b**: every test passes (the new tests of the phases done so far included). A red test from
  the known-flaky set is re-run alone before it is written off; `ShadingParity_AmbientAccumulation`
  is a real heap bug, never noise.
- **CP-d**: `0 differing, 0 missing` and an empty `MaxBlockAvgDiff` diff. **Any difference stops the
  phase**: list the image, the test and the diff, and report it. No reference is re-rendered, no
  threshold touched, no replay re-recorded.
- **CP-c**: PASS, with exactly the cumulative deletions of contracts/allowlist-end-state.md, no
  `STALE`, no `VIOLATION`, no `BADID`, and no new edge class.
- **The motion and camera labels**: `$CJ 028-pN-motion ctest --test-dir $W/build-028 -L '^motion$|^camera$' --output-on-failure`.

Phase-specific checks:
- **Phase 4** (camera): `grep -nE 'CRenderer::(imagePlane|invImagePlane|projection|focaldistance|aperture|dxdPixel|dydPixel)|fullImagePixels2camera' $W/src/ri/hiders/raytracer.cpp` prints nothing (US1 AS5). Time the three SC-008 scenes interleaved (as in phase 11).
- **Phase 5**: `grep -n 'cameraHasRotation\|relRotQ\|toWorld' $W/src/ri/geometry/*.{h,cpp}` prints only `motion.h` reading `frameParams().camera`.
- **Phases 6 to 11**: `grep -c 'CRenderer::invShutterTime' $W/src/ri/geometry/<family files>` is 0 for the phase's files.
- **Phase 11**: `grep -rn 'CRenderer::invShutterTime' $W/src/ri/geometry $W/src/ri/hiders` prints nothing (SC-001, US4 AS2); `grep -rln 'CRenderer::invShutterTime' $W/src` lists exactly 3 files (`rendererServicesImpl.h`, `rendererStatics.cpp`, `rendererFrame.cpp`; baseline 9). The new `FrameParams`/motion files carry the field name unqualified and are not counted. Time the SC-008 scenes interleaved with a starting-tree build kept in a scratch build directory (five alternating pairs per scene; validation.md §0.5): the current tree's median must not exceed the starting tree's by more than the starting tree's own spread.

## §3 Scope greps (any phase, and at the end)

```bash
# FR-017, FR-018, R15: files 028 never edits
git -C $W diff --stat 4eb683b -- src/ri/texture src/ri/render/rendererServicesImpl.h \
    src/libshader/shading/rendererServices.h src/libshader/shading/shading.h src/libshader/shading/shading.cpp \
    src/ri/hiders/reyes.cpp src/ri/hiders/reyes.h src/ri/hiders/stochastic.cpp src/ri/hiders/stochastic.h \
    src/ri/hiders/stochasticQuad.h src/ri/hiders/zbuffer.cpp src/ri/hiders/zbuffer.h src/ri/hiders/sampler.cpp \
    src/ri/hiders/sampler.h src/ri/geometry/object.cpp src/ri/geometry/csgDefinition.cpp \
    src/ri/geometry/implicit.h src/ri/geometry/dlo.h                     # expect empty
# surface.cpp: only CTesselationPatch::intersect's moving branch changes
git -C $W diff 4eb683b -- src/ri/geometry/surface.cpp | grep '^@@'
# FR-022: the allowlist only shrank
git -C $W diff 4eb683b -- tests/architecture/allowlist.txt | grep -c '^+[A-Z]'   # expect 0
# FR-003: the new State headers pull no core header
grep -nE '#include *"(.*/)?(renderer|rendererContext)\.h"' $W/src/ri/state/frameParams.* $W/src/ri/state/camera.* $W/src/ri/geometry/motion.h  # expect nothing
```

## §4 Linux (Ubuntu 24.04, GCC 13, `-Werror`): the maintainer's run

The plan agent has no Linux access. Use the established rsync + Maestri path (memory:
`feedback_gcc13_werror_linux_validation.md`; quote hosts as `${H}:path`).

**§4a Linux starting-tree baseline** (decision 1). **A hard gate at Checkpoint 2: it runs after
phase 2 and before any phase-3 edit**, on the worktree as it stands then (no `src/` change; the
phase-2 tests that need the new headers are `EXCLUDE_FROM_ALL`/`DISABLED`, and
`OPENRENDER_CAMERA_MOTION_ONLY_TESTS` still defaults to OFF). From phase 3 on, the tree no longer
builds the starting `src/` with these tests, so the Linux baseline cannot be taken later. Configure
with the default options, build, then, **in this order** (the replay must exist before the full suite, or
the eight `Motion_Replay_*` tests fail on Linux with "no replay file"):
1. `ctest -L architecture`, and save the full `archcheck` report as `$T/archcheck-base-linux.txt`;
2. `ctest -R '^Motion_ReplayRecord$'`, then copy `build/tests/unit/motion/replay-recorded-linux.txt`
   to `tests/unit/motion/replay/linux.txt` in the remote tree, and run `ctest -R '^Motion_Replay_'` (green);
3. stamp, `ctest -LE determinism-sweep -j2`, `image_manifest.sh record` into
   `$T/baseline-images-linux.sha256`, `diffs` into `$T/baseline-diffs-linux.txt`; a second run for
   the noise list `$T/baseline-image-noise-linux.txt`;
4. copy the four `tools/` files and the replay file back into the worktree.

Phase 3 does not start until these files are in the worktree.

**§4b Linux finished tree** (phase 12): the same §2 checkpoint with the `-linux` files: CP-a clean
under GCC 13 `-Werror`, CP-b green, CP-d `0 differing, 0 missing` against the Linux manifest,
CP-c PASS with the same 20 lines deleted, `ctest -L motion` green against `replay/linux.txt`,
`ctest -L '^determinism$' -j2` green, and the full sweep once, in the background:

```bash
$CJ 028-linux-sweep ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure > 028-linux-sweep.log
```

Every enabled sweep scene bit-identical across its configurations (SC-006); the result goes into
validation.md §7 beside the other Linux results.

**§4c Build options** (edge case "build options", phase 12, macOS and Linux): the default build
already compiles `ribVector`, `orender-wire` and whichever Python/Lua bindings the configuration
enables; in addition configure and build a JIT-off tree in a scratch directory:

```bash
$CJ 028-nojit-cfg cmake -B $W/build-028-nojit -S $W -DOPENRENDER_ENABLE_JIT=OFF
$CJ 028-nojit-build cmake --build $W/build-028-nojit --config Release -j4
$CJ 028-nojit-arch ctest --test-dir $W/build-028-nojit -L 'architecture|^motion$|^camera$' --output-on-failure
```

Expected: clean under `-Werror`, and the three labels green.

## §5 Before the merge only

```bash
$CJ 028-sweep ctest --test-dir $W/build-028 -L '^determinism-sweep$' -j2 --output-on-failure
```

Expected: every enabled sweep scene bit-identical across its configurations (SC-006). It cannot
judge change against master; CP-d does that.

## §6 Documentation check (phase 12)

- `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`: DB-009 Open with narrowed evidence and refreshed lines;
  DB-004 and DB-011 histories with spec 028 and the date; DB-011's size corrected
  (contracts/allowlist-end-state.md).
- The allowlist header counts equal the check's report; `metrics-baseline.txt` lowered only to
  reported values.
- `DEVNOTES.md`, `DEVNOTES_DETAILS/HIDER_PARITY.md` (the camera and motion paragraphs and the scope
  note about camera motion blur).
- No text mentions the merge.
