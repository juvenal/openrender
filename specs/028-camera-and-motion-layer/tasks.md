---

description: "Task list for spec 028: camera and motion layer"
---

# Tasks: Camera and Motion Layer

**Input**: design documents in `specs/028-camera-and-motion-layer/`.

**Prerequisites**:
- plan.md (phases 1-12, checkpoint procedure, test design, risks)
- spec.md (US1-US5, FR-001 to FR-026, SC-001 to SC-010, Clarifications of 2026-10-27 including the plan-stage decisions Q1-Q8)
- research.md (R1-R16)
- data-model.md
- contracts/frame-params.md, camera.md, motion-evaluator.md, replay-format.md, allowlist-end-state.md
- quickstart.md, validation.md (§0 plan-stage measurements on macOS), `tools/` (the macOS baselines and `image_manifest.sh`)
- `.specify/memory/constitution.md` (Principles III and VIII)

**Tests**: required. Constitution III (TDD) is non-negotiable. Two kinds of new test, with different red evidence (spec FR-024, maintainer decision Q3):
- **Unit tests, red first by build failure** on the starting tree: the 6 `camera`-label tests (`Camera_Analytic`, `Camera_StartingTreeEquivalence`,
  `Camera_CropInvariance`, `Camera_GenerateRayOnly`, `Camera_LensModel`, `Frame_DefaultValue`), the 4 State-only `motion`-label tests
  (`Motion_TwoKeyModel`, `Motion_Isolation`, `Motion_CameraSlerpEquivalence`, `Motion_TransformRayEquivalence`) and `Frame_ParamsEqualCore`.
- **Characterization tests, green on the starting tree by construction**: the 8 `Motion_Replay_<family>` tests compare the finished tree
  with a replay file **recorded on the starting tree** (a replay is data, not code). Their red evidence is a **mutation per family** in a
  scratch copy (spec 025's T-P1a precedent), taken in Phase 1 (T016). Every existing guard is also green by design.

**Known-red tests (the only allowed failures)**: **none.** No test is allowed to be red at any checkpoint. A new test whose API does not
exist yet is registered `EXCLUDE_FROM_ALL` + `DISABLED` (see "Red-first convention"), so it shows as "Not Run (Disabled)", never as a failure.
The only other non-passes at a checkpoint are `LibShader_Format_rslo_lower_minor` (it skips itself, validation.md §0.3) and a known-flaky
test that passes when re-run alone (below).

**Organization**: the phases follow plan.md's phases 1-12 exactly. User-story labels (US1-US5) are on the tasks that serve a story,
including the Phase 2 test tasks and the per-phase checkpoints (a checkpoint's CP-d is US3's acceptance). Setup, baseline and Linux-handover
tasks carry no label. Every phase ends in a **checkpoint task** and then a review pause: the agent stages its changes with `git add`,
summarizes, and waits for the maintainer.

**Never commit.** Stage with `git add` only. Decline any spec-kit auto-commit hook (`speckit.git.commit` before and after tasks, implement and
the other stages). No GitHub writes: #34 is closed by the maintainer's `Closes #34` line in the merge commit; #106, #107 and #108 are already filed.

**Formatting**: run clang-format with the repo's `.clang-format` on an edited file only if it was clang-format clean before the edit (check
with `clang-format --dry-run` on the unedited file first); otherwise leave the formatting alone.

**Working directory**: `/Volumes/Projects/Development/CLI/openrender-worktrees/028-camera-and-motion-layer` (branch
`028-camera-and-motion-layer`; base = master `4eb683b`, spec commit `8076a15`, plan commit `dc14044`). Work only in this worktree; the one scratch tree (`X`) is a
gitignored directory inside it, as the project rules require. The macOS build tree is `build-028` (configured at the starting tree, Release, JIT on), logs in
`build-028-logs/` (both gitignored). Paths are repo-relative. Shell variables, as in quickstart.md:

```bash
W=/Volumes/Projects/Development/CLI/openrender-worktrees/028-camera-and-motion-layer
T=$W/specs/028-camera-and-motion-layer/tools
CJ=~/.claude/bin/cpujob
X=$W/build-028-scratch      # scratch copy of the tree (T016): inside the worktree, gitignored by `build-*`
```

---

## Standing rules (apply to every task)

**Builds and tests (marked [RUN])**:
- The implementer asks the coordinator before each [RUN], then runs it through `~/.claude/bin/cpujob <label> <command>`: one job at a time
  machine-wide (spec 027's agent shares the lock), `nice -n 19`, at most `-j4`; the determinism sweep runs at `-j2`.
- Long jobs go in the background with output redirected to `build-028-logs/<label>.log`. Wait for the completion notice.
- Never run a test binary directly (a hung one has no timeout). Never use `-j8`, a bare `-j`, or a count above 4.
- A plain `cmake --build` / `ctest` line below always means the `cpujob` form. No agent runs anything on the Linux machine: Linux steps are
  handed to the coordinator (T036, T111).

**Checkpoint procedure** (referenced as CP-a to CP-d by every phase's last task; commands as in quickstart.md §2). Defined once, here:
- **CP-a build**: `$CJ 028-pN-build cmake --build $W/build-028 --config Release -j4`. It must be clean under `-Werror`.
- **CP-b suite**: stamp first (`$T/image_manifest.sh stamp $W/build-028`), then
  `$CJ 028-pN-suite ctest --test-dir $W/build-028 -LE determinism-sweep -j4 --output-on-failure`, then copy the log before the next ctest run
  overwrites it: `cp $W/build-028/Testing/Temporary/LastTest.log $W/build-028-logs/pN.LastTest.log`.
  - Expected: every test passes (838 on the starting tree, plus the tests this spec adds, so far as they are enabled). No known-red set.
  - A red test from the known-flaky set (memory `reference_ctest_flaky_tests.md`: `ImagerRegressionRender`, `Parity_transparency-matte-aov`,
    `Parity_depthdefault`, `ShadingParity_AmbientAccumulation`) is re-run alone before it is written off. `ShadingParity_AmbientAccumulation`
    is a real heap bug, never noise: re-run it under the same conditions as the baseline and compare.
- **CP-c architecture**: `$CJ 028-pN-arch ctest --test-dir $W/build-028 -L architecture --output-on-failure`, plus the full report
  `$CJ 028-pN-archreport $W/build-028/tests/architecture/archcheck --source-dir $W --build-dir $W/build-028 --config $W/tests/architecture`.
  Record the `ALLOWED`, `METRIC` and `RESULT` lines in validation.md §1. Expected `ALLOWED` after each phase (macOS), cumulative:

  | After phase | DB-004 | DB-011 | Lines deleted so far |
  |---|---|---|---|
  | 1-3 | 65 | 173 | 0 |
  | 4 | 65 | 166 | 7 |
  | 5 | 59 | 166 | 13 |
  | 6 | 58 | 166 | 14 |
  | 7 | 57 | 166 | 15 |
  | 8, 9, 10 | 56 | 166 | 16 |
  | 11, 12 | 52 | 166 | 20 |

  - Expected otherwise: `RESULT PASS`, no `STALE`, no `VIOLATION`, no `BADID`, no new edge class, no unclassified file. The new State, Geometry
    and core files add allowed edges only (Geometry -> State, Hiders -> State, Core -> State/Hiders; contracts/allowlist-end-state.md).
  - **Why the camera/motion State-only targets are behind an option**: archcheck's `loadMakefiles` walks every `DependInfo.cmake`,
    `EXCLUDE_FROM_ALL` targets included, and exits 2 "incomplete build tree" when an in-scope object (any source under `src/`) is missing. The
    State-only library compiles `src/ri/state/frameParams.cpp` and `camera.cpp` a second time, so those two files join it only behind the CMake
    option `OPENRENDER_CAMERA_MOTION_ONLY_TESTS`, **off until T045 (Phase 3)**, which creates the sources (research R16). Until then CP-c sees only
    production units.
- **CP-d byte-identical output** (every phase from 3 on, for **all** suite images, not only the camera, motion and DOF scenes): after CP-b and
  before any other ctest run, `$T/image_manifest.sh diff $T/baseline-images-macos.sha256 $W/build-028 $T/baseline-image-noise-macos.txt` must
  report **0 differing and 0 missing images**, and
  `$T/image_manifest.sh diffs $W/build-028-logs/pN.LastTest.log | diff - $T/baseline-diffs-macos.txt` must be empty (every `MaxBlockAvgDiff`
  equal). The baseline is the starting tree's 761 images and 389-test table; the noise list holds two scratch files
  (`tests/shading_parity/ri.tif`, `tests/shading_parity/test.tif`), which stay in it (decision Q5; no 028 task edits those test registrations),
  and a noise-listed image's `MaxBlockAvgDiff` must still match. Any difference stops the phase: it is a behaviour change in a
  behaviour-preserving phase (FR-014).
- **CP-m motion and camera labels**: `$CJ 028-pN-motion ctest --test-dir $W/build-028 -L '^motion$|^camera$' --output-on-failure` (anchored:
  `-L` is a regular expression, and an unanchored `motion` also selects the `motion-record` label).
- **Per platform**: macOS runs CP-a to CP-d at every phase; Linux runs them against its own `-linux` baseline files at Phase 12 only (T111).
  There is no cross-platform bit comparison (decision 1).

**Allowlist rules (FR-022)**: allowlist lines are identified by **file and symbol (the key), never by line number**: the line numbers in
contracts/allowlist-end-state.md are `4eb683b`'s, and another spec's merge can shift them. The only allowlist change in 028 is **deleting the
20 lines of contracts/allowlist-end-state.md**, each at the start of the phase that names it, plus any line the check reports `STALE`.
**Adding, re-keying or swapping a line is never allowed.** After a phase's deletion, `ctest -L architecture` must report exactly that phase's
edges as `VIOLATION` and nothing else (T-A1); any other violation means the design is wrong: stop and report. `metrics-baseline.txt` only goes
down, and only to values the check prints (T102).

**Thresholds and references**: never widen a threshold, never disable a test (other than the red-first `DISABLED` markers below, each removed by
the phase that provides the API), never regenerate a reference image, never re-record a replay file from a changed tree, and never add a path to
the noise list to get a green result. **There is no output-changing phase in 028** (plan Summary): FR-014 holds at every checkpoint.

**Red-first convention for tests that cannot compile yet**: a new test that needs an API that does not exist yet is added with
`EXCLUDE_FROM_ALL` on its executable **and** `set_tests_properties(<name> PROPERTIES DISABLED TRUE)` on its test, so the main build and every
checkpoint stay green.
- The red state is shown by a build failure of the target (the error text goes in validation.md §2); the task that provides the API removes both
  markers, builds, and runs the test green.
- **The camera/motion State-only tests are different** (research R16): they link the State-only library, which compiles `src/ri/state/*.cpp`
  a second time. The two new State files join it, and the tests are defined, only when the CMake option `OPENRENDER_CAMERA_MOTION_ONLY_TESTS`
  is on (it also needs `OPENRENDER_STATE_ONLY_TESTS`, already ON). The option is **OFF by default until T045**. Until then the two sources are
  appended behind `if(EXISTS ...)` so the red evidence is a per-test compile failure (`camera.h`, `frameParams.h`, `motion.h` not found), not one
  configure error; T045 removes the guard. Red evidence is taken in a scratch build directory with the option ON (T033).
- `Frame_ParamsEqualCore`, the replay tool, its two fixture plug-ins and the `Motion_Replay_*` tests compile no `src/` source and need no option.
  `Frame_ParamsEqualCore` is plain `EXCLUDE_FROM_ALL` + `DISABLED` until T045; the replay tool is enabled from Phase 1 (it uses starting-tree
  interfaces only).

**Mutation check**: every new test gets a scratch mutation at its green point (edit, rebuild, show the named test red, revert, confirm
`git diff -- <file>` empty; results in validation.md). Replay mutations run in the scratch copy `$X` (T016); the unit-test
mutations run in the worktree on staged files (T047 to T049):
- `motion_replay` families, T016: `quadrics`, `raytransform`, `bilinear`, `polygons`, `implicit`, `tessellation` (intersection side and sample
  side), `gridonly`, `emptyshutter`;
- `Camera_*`, `Frame_*`: T047 and T049; `Motion_*`: T048.

**No task depends on the merge**: no post-merge step, no merge-commit hash, no GitHub comment. DB-009 stays **Open** (its Reyes side is spec 031)
and is never called Resolved; DB-004 and DB-011 record their deleted lines and the date the task is done. Spec 027 (texture system) is on another
branch; its textual overlap (`renderer.cpp`, `src/ri/CMakeLists.txt`, the allowlist, `metrics-baseline.txt`, `DOMAIN_BOUNDARIES.md`) is the
maintainer's merge, not a task, and no 028 task depends on it.

**Files 028 never edits** (FR-017, FR-018, plan "Not edited"; `git diff --stat 4eb683b` on them must stay empty, checked by T050, T109):
`src/ri/texture/`, `src/ri/render/rendererServicesImpl.h`, `src/libshader/shading/rendererServices.h`, `shading.h`, `shading.cpp`,
`src/ri/render/renderer.h`, `src/ri/render/rendererStatics.cpp`, `src/ri/render/rendererContext.cpp`, every Reyes and zbuffer hider file
(`reyes.cpp`, `reyes.h`, `stochastic.cpp`, `stochastic.h`, `stochasticQuad.h`, `zbuffer.cpp`, `zbuffer.h`), `src/ri/hiders/sampler.{h,cpp}`,
`src/ri/geometry/object.cpp`, `csgDefinition.cpp`, `implicit.h`, `dlo.h`, and `src/ri/geometry/surface.cpp` outside
`CTesselationPatch::intersect`'s moving branch. No task edits the `tests/shading_parity` test registrations (Q5).

**Stop conditions** (stop and report, never work around):
- any CP-d difference, any `MaxBlockAvgDiff` difference, any replay difference, or an unexpected `VIOLATION` in a phase from 3 on (plan "Stop rule");
- a new State, Geometry or Hider file would need a header that Principle VIII's table forbids, or `motion.h`/`camera.h` would need `renderer.h`,
  `rendererContext.h`, `shading.h` or a hider header (FR-003, FR-012);
- archcheck mishandles a doubly compiled State unit (R16);
- a replay mutation does not turn its family's test red (the replay is then too weak: strengthen the scene in Phase 1; a bitwise-equivalent mutation gets a named substitute only with the maintainer's approval, T016);
- master (or any other branch) is merged into this branch while the work is in progress: the baselines are of `4eb683b`, so the maintainer decides;
- a Linux-only starting-tree failure, or GCC 13 `-Werror` rejecting new test code after the maintainer approved the tests (T036, T037).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency on unfinished tasks).
- **[Story]**: US1 (every raytrace camera ray comes from one camera layer), US2 (every primitive takes its motion from one evaluator), US3 (every
  motion and depth-of-field scene renders as before), US4 (Geometry and the raytrace hider stop reading shutter and camera statics), US5 (the
  camera layer is ready for Reyes and for a bidirectional integrator).
- **[RUN]**: a build or test run done by the implementer, one job at a time, after asking the coordinator.

---

## Phase 1: Setup and baselines (before ANY `src/` edit)

**Purpose**: confirm the plan-stage macOS baselines, build the characterization tool and record the starting tree's replay file, show each
family's mutation red, and re-confirm the image manifest with the new tests in the build. **No file under `src/` changes in this phase.** The plan-stage
numbers are in validation.md §0 and `tools/`.

- [X] T001 Fill the "Implementation log" of `specs/028-camera-and-motion-layer/validation.md` (§1 is stubbed: "Filled in by the implement stage") and create the skeleton of the sections later tasks write into, so none is invented mid-phase:
  one heading and a one-line purpose each, empty tables, in this order: **§2** red evidence (T033), **§3** the Linux baseline gate (T036, T037), **§4** unit-test mutations, greps and diffs (T047 to T050),
  **§5** the allowlist `VIOLATION` lines and the timing tables (T052, T055, T056, T058, T062, T074, T080, T092, T100), **§6** the final macOS pass (T109, T110), **§7** the Linux finished-tree results, the Linux determinism sweep included (T111).
  - Record the base `4eb683b`, the worktree HEAD (`git rev-parse HEAD`), `build-028`, and `X`.
  - One table row per phase for the checkpoint results (CP-a, CP-b totals, CP-c `ALLOWED`/`METRIC`/`RESULT`, CP-d counts, `-L '^motion$|^camera$'`).
  - State the rule "no known-red test" and that no phase changes output.
- [X] T002 [P] Confirm, without editing, that the plan-stage baselines are intact and the tree is the starting tree:
  - `$T/image_manifest.sh` is executable; `$T/baseline-images-macos.sha256` has 761 lines; `$T/baseline-diffs-macos.txt` has 389 lines;
    `$T/baseline-image-noise-macos.txt` names exactly `tests/shading_parity/ri.tif` and `tests/shading_parity/test.tif`; `$T/archcheck-base-macos.txt`
    shows `DB-004 x 65`, `DB-011 x 173`, `RESULT PASS`.
  - `git diff --stat 4eb683b -- src tests CMakeLists.txt CMake shaders examples` prints nothing.
  - `grep -E 'CMAKE_BUILD_TYPE|OPENRENDER_ENABLE_JIT|VCPKG_TARGET_TRIPLET' build-028/CMakeCache.txt` shows Release, JIT ON, `arm64-osx-openrender`.
  - Record in validation.md §1. If any check fails, stop and report.
- [X] T003 [RUN] CP-a on the unchanged tree (an incremental build; expect "up to date" or a clean rebuild). Record the outcome.
- [X] T004 Create `tests/unit/motion/CMakeLists.txt` and `tests/unit/motion/replay/` and wire the directory: `add_subdirectory(unit/motion)` in
  `tests/CMakeLists.txt` **after** `add_subdirectory(unit/state)` (line ~67).
  - Define the executable `motion_replay` (`EXCLUDE_FROM_ALL` not needed: it builds on the starting tree), `cxx_std_20`, linking `ribRender
    openrendercommon`, with the include directories and `ENVIRONMENT` of `tests/unit/csg/CMakeLists.txt` (same in-process Ri sessions).
    `ORENDERHOME` must be `${CMAKE_SOURCE_DIR}`, never `tests/visual`: that directory's `.orenderrc` sets a Display and Format, so `RiWorldEnd` would try to open the file display, and a failed display
    load followed by another `RiBegin`/`RiEnd` cycle in the same process crashes (GitHub #2; the tool runs many sessions in one process). Say so in a CMake comment, as `tests/unit/csg/CMakeLists.txt` does.
  - Define the fixture plug-ins `implicit_moving_sphere` and `dlo_sphere` as `MODULE` libraries from `fixtures/` (sources written by T010, T011),
    built into the build tree; they implement the unchanged `implicit.h` and `dlo.h` ABIs and compile no `src/` source.
  - Register `Motion_ReplayRecord` (label `motion-record`, `TIMEOUT 300`) running `motion_replay --record
    ${CMAKE_CURRENT_BINARY_DIR}/replay-recorded-<platform>.txt`; the `Motion_Replay_<family>` registrations come with T015.
  - `<platform>` is `macos` or `linux`, chosen in CMake from `CMAKE_SYSTEM_NAME` (`Darwin`, `Linux`); any other value is a configure error.
  - Comment: recording writes **only into the build tree**, so running it on a later tree cannot overwrite the tracked replay file
    (contracts/replay-format.md rule 2).
- [X] T005 [US2] Write `tests/unit/motion/replay_capture.h`: the capture context and the in-process session helper, modelled on
  `tests/unit/csg/csgTestUtils.h`.
  - Its `addObject` **calls `CRendererContext::addObject` first** (so the camera-motion synthesis of `rendererContext.cpp:686-760` runs as in
    production), then attaches and records the object. This is the plan's "replay harness fidelity" risk; `Motion_Replay_raytransform`'s mutation
    (T016) proves it is exercised.
  - One fresh `RiBegin`/`RiWorldBegin` ... `RiWorldEnd`/`RiEnd` session per scene (gotcha 20: the lexer is reset by `riblex_destroy()` only for RIB parsing, which this tool does not use). Copy the session lifetime of `CCsgSession` in `csgTestUtils.h` (`RiBegin`, `RiWorldBegin` in the constructor; captured objects detached, then `RiWorldEnd`, `RiEnd` in the destructor), and
    obtain the shading context as `CRenderer::contexts[0]` as it does.
  - **Every `intersect()`, `sample()` and `materialize()` call in every scene runs while the session is open, between `RiWorldBegin` and `RiWorldEnd`, and never after `RiWorldEnd`.** The frame values are installed in `beginFrame` and uninstalled in `endFrame`: on the starting tree a query made after the world block would still read the `CRenderer` statics and record "fine", but from Phase 5 on `transformRay` and `toShutterRate` would read the *default* `FrameParams` (no camera rotation, `invShutterTime = 1`) and the replay
    would go red with no cause in the migrated code, while the stop rules forbid re-recording. Captured objects may also be released at `endFrame` (gotcha 17).
  - **Starting-tree interfaces only** (contracts/replay-format.md rule 1): the Ri API, `CObject::intersect()`, `CObject::sample()`,
    `CObject::materialize()`. The file includes none of `frameParams.h`, `camera.h`, `motion.h` (`grep` guard in T014).
- [X] T006 [US2] Write `tests/unit/motion/motion_replay.cpp`, core only (no scenes yet): argument parsing `--record <file>`, `--check <file>`,
  `--family <f>`; the platform name; the query grid; the writer and the checker.
  - **Header line**: `# motion_replay v1 <platform> master-4eb683b`. **Records**: `<scene> <family> I <leaf> <ray> <time> <hit> <t> <u> <v> <Nx> <Ny> <Nz>` and
    `<scene> <family> S <leaf> <uv> <time|BEGIN|END> <Px> <Py> <Pz> <Ngx> ... <dPdtimez>`; every float as a C99 hex float (`%a`); sorted by scene, family,
    kind, leaf, ray/uv, time (contracts/replay-format.md "Format").
  - **Intersection queries**: a fixed grid of 32 camera-space rays **per leaf** (an 8x4 lattice in front of the leaf's bound, directions toward and past it, the last two slots grazing rays) at times
    `-0.25, 0, 0.25, 0.5, 0.75, 1, 1.25`, with a fixed orthographic ray footprint cycling over four widths (a footprint of zero asks `CSurface` for its finest tessellation, a tree of hundreds of thousands of nodes);
    each record holds hit flag, and for a hit `t`, `u`, `v`, `N[3]`, and the leaf's index in capture order. The tool walks the hierarchy under the leaf as `trace()` does.
  - **Sample queries**: per leaf a 3x3 `(u, v)` grid (2x2 for the two large subdivision meshes) at the same seven times, then with `PARAMETER_BEGIN_SAMPLE` and with `PARAMETER_END_SAMPLE`, requesting `P`,
    `Ng`, `dPdu`, `dPdv`, and `dPdtime` **only with `PARAMETER_BEGIN_SAMPLE`** (the patch-grid families read outside their arrays for `dPdtime` at an interpolated time and at END: an existing defect, GitHub #110, contracts/replay-format.md);
    varyings set up as `surface.cpp:159-172` does, with zeroed storage in every slot; no primitive variables.
  - **Queries happen inside the frame** (T005); the tool asserts a world block is open before each query.
  - **Check mode**: prints the first 10 differing lines per family and exits 1 on any difference, any missing line or any extra line. With `--family`, only that
    family's lines are compared. A missing replay file for the platform fails with "no replay file for <platform>" and is never skipped.
  - **Family tag** of a record = the `Motion_Replay_<family>` test that guards the code path it exercises: `quadrics`, `raytransform`, `bilinear`, `polygons`,
    `implicit`, `tessellation`, `gridonly`, `emptyshutter`. The scene-to-family map is in T007 to T013.
- [X] T007 [US2] In `motion_replay.cpp` add scenes `quadrics-deform` (shutter `0 0.5`, static camera: the 7 quadrics, each with `MotionBegin` around two parameter
  sets) and `quadrics-xform` (shutter `0.25 0.75`: the 7 quadrics under `MotionBegin` `Translate` + `Rotate`). Family `quadrics`. The shutters make
  `invShutterTime` 2 and 2 (not 1), so a wrong scale is visible.
- [X] T008 [US2] Add the camera-motion scenes (shutter `0 1`, `MotionBegin` before `WorldBegin`): `quadrics-camera-translate`, `quadrics-camera-rotate`,
  `quadrics-camera-rotonly` (static quadrics, camera motion synthesized) and `quadrics-own-motion-camera` (a quadric with its own `MotionBegin` `Translate`
  under a moving camera: **GitHub #106, no camera motion blur, pinned**). Family `raytransform`.
- [X] T009 [US2] Add scenes `bilinear` (a moving `Patch "bilinear"`, shutter `0 0.5`; family `bilinear`, both `I` and `S` records, so the `dPdtime` precedence defect of
  `patches.cpp:477`, GitHub #108, is pinned) and `bicubic`, `nurbs` (a moving bicubic patch and a moving NURBS, intersected through `CSurface::intersect`, whose
  moving-tessellation branch is the phase-10 code; family `tessellation` for both `I` and `S` records: the `S` records pin the `patches.cpp` sample paths of phase 7).
- [X] T010 [P] Write `tests/unit/motion/fixtures/implicit_moving_sphere.cpp`: a test-only implicit-surface plug-in (the unchanged `implicit.h` ABI) whose sphere centre
  moves with the time argument it receives, so `CImplicit`'s three plug-in time reads are observable. It is the first coverage of `CImplicit` (research R14).
- [X] T011 [P] Write `tests/unit/motion/fixtures/dlo_sphere.cpp`: a test-only dynamic-load-object plug-in (the unchanged `dlo.h` ABI) with a sphere intersector, so
  `CDLObject`'s ray `transform()` is observable under camera translation and rotation. First coverage of `CDLObject`.
- [X] T012 [US2] Add scenes `polygons` (a moving triangle and a moving quad via `PointsPolygons`, shutter `0 0.5`; family `polygons`), `implicit` (the fixture
  plug-in, shutter `0 1`; family `implicit`) and `dlo-camera-translate`, `dlo-camera-rotate` (the fixture plug-in under a moving camera; family `raytransform`).
- [X] T013 [US2] Add scenes `curves`, `points`, `subdiv-grid` (an open Catmull-Clark grid with `interpolateboundary`: `CPatchGrid` border faces and `CBicubicPatch` interior faces) and
  `subdiv-cube` (a cube with a creased edge: `CBSplinePatchGrid` strips and `CSubdivision` corner patches; `sample()` only), all moving, shutter `0 0.5`, family `gridonly`.
  **A `PatchMesh` does not instantiate `CPatchGrid` or `CBSplinePatchGrid`** (only the subdivision creator does, `subdivisionCreator.cpp`), so the planned `patchmesh` and `bsplinemesh` scenes were replaced; and `empty-shutter` (a moving sphere, shutter `0.5 0.5`, `invShutterTime` 1 and motion
  sampling cleared; family `emptyshutter`).
- [X] T014 [US2] [RUN] Build `motion_replay` and the fixtures (CP-a), then record through ctest, never by running the binary:
  `$CJ 028-p1-replay ctest --test-dir $W/build-028 -R '^Motion_ReplayRecord$' --output-on-failure`.
  - Run it **twice** and compare the two files byte for byte: the tool is deterministic (no thread, no clock). Any difference: stop.
  - Guard against a vacuous replay (it would test nothing): for every family that has `I` records (all but `gridonly`, whose primitives are not raytraced), the file holds at least one `hit=1` and one `hit=0` `I` record,
    and in every moving scene some record at time 0 differs from the same record at time 1 (for `curves` and `points`, whose `sample()` selects a key and ignores time, some `BEGIN` record differs from the `END` one);
    list the per-family counts in validation.md §1. Also run the tool's own coverage guard (it fails the scene if the expected leaf classes, moving or transform-motion leaves are absent).
  - `grep -nE 'frameParams\.h|camera\.h|motion\.h' tests/unit/motion/motion_replay.cpp tests/unit/motion/replay_capture.h` prints nothing (starting-tree interfaces only).
- [X] T015 [US2] [RUN] **On the starting tree only**: copy `build-028/tests/unit/motion/replay-recorded-macos.txt` to `tests/unit/motion/replay/macos.txt` (a deliberate step; never again).
  - Register the 8 tests `Motion_Replay_quadrics`, `_raytransform`, `_bilinear`, `_polygons`, `_implicit`, `_tessellation`, `_gridonly`, `_emptyshutter` (label `motion;unit`, `TIMEOUT 120`),
    each running `motion_replay --check <replay>/<platform>.txt --family <f>`.
  - `$CJ 028-p1-motion ctest --test-dir $W/build-028 -R '^Motion_Replay_' --output-on-failure`: **8/8 green by construction**.
- [X] T016 [US2] [RUN] Replay mutations (red evidence for the characterization tests), in the **scratch copy `$X`** (never in the worktree's own `src/`).
  - Create `X` (a gitignored directory inside the worktree, accepted by the maintainer 2026-10-27: the project rule says to work only inside the worktree) by `mkdir -p $X && rsync -a --exclude '/.git' --exclude '/build*' $W/ $X/tree/` (both patterns are **anchored** to the transfer root, so only the worktree's top-level `build-028*` directories and `.git` are skipped and no `build*`-named file or directory deeper in the tree is lost; the exclude of `/build*` also keeps `$X`, which is `$W/build-028-scratch`, from copying itself); configure `$X/bld` like `build-028` (copy from `build-028/CMakeCache.txt`: `CMAKE_GENERATOR`,
    `CMAKE_BUILD_TYPE`, `CMAKE_TOOLCHAIN_FILE`, the `VCPKG_*` settings, `BISON_EXECUTABLE`, `LLVM_DIR`, `CMAKE_OSX_DEPLOYMENT_TARGET`); build the targets the replay needs. One full build, then
    incremental ones; ask the coordinator.
  - For each mutation edit one file in `$X/tree`, rebuild, run `ctest -R '^Motion_Replay_'`; **exactly the named family's test must turn red and the other seven stay green**; revert the file
    (`diff -r $W/src $X/tree/src` empty) before the next:
    1. `quadrics`: drop the `(double)` cast of one quadric parameter in `CSphere::intersect` (`quadrics.cpp`, near `:200-203`);
    2. `raytransform`: drop the conjugate in the ray SLERP of `transform()` (`objectMisc.h`, `:209-214`);
    3. `bilinear`: swap the operands of the `interpolatev` call in `CBilinearPatch::intersect` (`patches.cpp:228-234`);
    4. `polygons`: swap the operands of one `interpolatev` in `polygons.cpp:183-187`;
    5. `implicit`: replace one `ray->time` passed to the plug-in by `0.0f` (`implicitSurface.cpp:142`);
    6. `tessellation` (intersection side): swap the operands of one `interpolatev` in `CTesselationPatch::intersect` (`surface.cpp:1453-1600`);
    7. `tessellation` (sample side): swap the key operands of one bicubic `sample()` lerp (`patches.cpp:692-695`);
    8. `gridonly`: replace `CRenderer::invShutterTime` by `1.0f` in `curves.cpp`'s `sample()` (`:385`);
    9. `emptyshutter`: in a quadric's `sample()`, replace the read of `CRenderer::invShutterTime` by `1.0f / (CRenderer::shutterClose - CRenderer::shutterOpen)` (decision 5: the stored reciprocal is never recomputed).
  - A mutation that does **not** turn its test red usually means the replay is too weak: strengthen the scene (more rays, times or sample points; the starting tree is unchanged, and the macOS
    replay is not frozen until Checkpoint 2), re-record (T014, T015) and repeat. If the mutation is **provably bitwise-equivalent for every input** (for example a cast that changes no bit), a scene cannot help: propose a **named substitute mutation** for that family to the coordinator, use it only with the maintainer's approval at that time (decision 2026-10-27), and record both in validation.md. Never weaken a mutation silently to make it red.
  - Record each mutation, the failing test and the first differing line in validation.md §1. `X` stays (T017).
- [X] T017 [US3] [RUN] Make `$X` the **starting-tree reference build** for SC-008 (T056, T100): confirm `diff -r $W/src $X/tree/src` and `diff -r $W/CMakeLists.txt $X/tree/CMakeLists.txt` are
  empty (every mutation reverted, and the tree is `4eb683b`'s `src/`), build `all` in `$X/bld`, and time each of `Visual_teapot-motion-raytrace`, `Convergence_dof-raytrace`
  and `Visual_blobby-motion-grow-raytrace` once through `cpujob` to prove the tree runs them. `$X/tree/src` is never edited again.
- [X] T018 [US3] [RUN] Re-confirm the manifest with the Phase 1 tests in the build (they add tests, not images): stamp, CP-b (`028-p1-suite`: the plan's 838 tests plus the 9 new `motion`
  tests, all green), CP-d against the plan-stage baselines (**0 differing, 0 missing**; the `MaxBlockAvgDiff` table equal), and CP-c (`DB-004 x 65`, `DB-011 x 173`, PASS). Record in validation.md §1.
  If a baseline image differs, a Phase 1 test changed output: stop and report.
- [X] T019 **Checkpoint 1 (PAUSE 1)**: the baselines and the starting tree's replay are recorded. Stage `validation.md` (§1), `tests/unit/motion/**`, `tests/CMakeLists.txt`; summarize the replay's
  per-family counts and the nine mutations; wait for review.

---

## Phase 2: Tests first, then the Linux baseline gate

**Purpose**: Constitution III. After this phase every new test exists and is red by build failure (the unit tests) or green by construction with its mutation shown (the replays,
Phase 1). **No file under `src/` changes in this phase.** Phase 2 ends in the maintainer's approval of the tests and then in a **hard gate (decision Q8)**: the Linux starting-tree baseline
is taken on the approved tree, and phase 3 does not start until its files are in the worktree.

- [X] T020 [US1] Scaffolding, in `tests/unit/state/CMakeLists.txt`, `tests/unit/camera/CMakeLists.txt` (new) and `tests/CMakeLists.txt`:
  - In `tests/unit/state/CMakeLists.txt`, before `add_subdirectory` order matters (it is processed first): `include(CMakeDependentOption)` and
    `cmake_dependent_option(OPENRENDER_CAMERA_MOTION_ONLY_TESTS "Build the camera and motion State-only tests" OFF "OPENRENDER_STATE_ONLY_TESTS" OFF)`
    (OFF until T045; the dependency makes it OFF whenever `OPENRENDER_STATE_ONLY_TESTS` is OFF, so no configuration needs a `FATAL_ERROR`; the plain-`option` form defaulting to `${OPENRENDER_STATE_ONLY_TESTS}` is equivalent once T045 flips it). Inside the `OPENRENDER_STATE_ONLY_TESTS` block, when the option is ON, append
    `${CMAKE_SOURCE_DIR}/src/ri/state/frameParams.cpp` and `camera.cpp` to `state_only_sources`, each guarded by `if(EXISTS ...)` until T045.
  - Stage the include staging directory: besides `ri_config.h`, add a `configure_file(... COPYONLY)` copy of `src/ri/core/ray.h` (`CRay`, Infra) to `state_only_include`, as spec 025 staged `ri_config.h`, so
    `Motion_Isolation` can include it without putting `src/ri/core` (which holds the Shading shims) on the path. If `ray.h` pulls anything that does not resolve on the narrow path, **stop and report** (R12-style).
  - `tests/unit/camera/CMakeLists.txt`: defined only when the option is ON; the helper `add_camera_motion_test(NAME EXE LABEL SOURCES... [ENABLED])` (same convention as `add_state_only_test`:
    `EXCLUDE_FROM_ALL` + `DISABLED` unless `ENABLED`; links `state_only_sources`; `TIMEOUT 60`; label `camera` or `motion`; a test's own extra include directory, e.g. `src/ri/geometry` for the motion tests,
    but never `src/ri/render`, `src/libshader`, `src/ri/hiders` or `src/ri/core`). Add `add_subdirectory(unit/camera)` to `tests/CMakeLists.txt` after `unit/state` and before `unit/motion`.
- [X] T021 [P] [US1] Write `tests/unit/camera/test_camera_analytic.cpp` (`Camera_Analytic`, State-only; US1 AS1-AS3): the centre pixel's pinhole ray is `(0,0,1)` from the origin; an orthographic ray starts on
  the film point and points along +z with `da = 0`, `db = dxdPixel`; for a thin lens every lens point gives a ray through the pinhole ray's focal-plane point, with the origin offset by the lens point scaled by the
  aperture radius. Builds its `FrameParams` by hand (no renderer core).
- [X] T022 [P] [US1] Write `tests/unit/camera/test_camera_equivalence.cpp` (`Camera_StartingTreeEquivalence`, State-only; FR-006, FR-014): `generateRay` against a **verbatim copy, compiled in the test's own translation unit**, of
  `computeSamples`' arithmetic (`raytracer.cpp:626-701`) and `fullImagePixels2camera` (`renderer.h:618-631`), `memcmp` on `from`, `dir`, `da`, `db`, over a film grid (including negative and out-of-window positions),
  lens points on and off the disk edge, both projections, DOF on and off, and three screen windows. Cite file and line in comments (research R9: expression shapes, `from[X] += lensU * aperture` as a compound assignment).
- [X] T023 [P] [US1] Write `tests/unit/camera/test_camera_crop.cpp` (`Camera_CropInvariance`, State-only; US1 AS4): two `FrameParams` that differ only in the crop origin (`sampling.originX/Y`) give identical rays for the same absolute film position;
  and a screen-window origin change does change the ray (the test can tell the two apart).
- [X] T024 [P] [US5] Write `tests/unit/camera/test_camera_generate_ray_only.cpp` (`Camera_GenerateRayOnly`, State-only; FR-009, FR-010, SC-010, US5 AS2-AS3): a test double implementing **only** `generateRay` compiles against `ICamera` and runs;
  `sampleRay(CameraSample)` forwards to it with the same arguments; `lensModel()` is a pinhole; `evaluateImportance` and `sampleConnection` return `CameraSupport::NotSupported` and leave a sentinel-filled output untouched
  (the output types are only forward-declared, so the test passes references to opaque storage or relies on incomplete types as the contract allows; follow contracts/camera.md).
- [X] T025 [P] [US5] Write `tests/unit/camera/test_camera_lens_model.cpp` (`Camera_LensModel`, State-only; FR-007, US5 AS1): `lensModel()` returns the projection, image plane, aperture radius, focal distance and its reciprocal, focal length and f-stop of the
  frame; `thinLens()` is `aperture != 0`; `cocFactorScreen()` equals a verbatim copy of `renderer.cpp:447`'s expression, `memcmp`, and is 0 for a pinhole.
- [X] T026 [P] [US1] Write `tests/unit/camera/test_frame_default.cpp` (`Frame_DefaultValue`, State-only): with nothing installed `frameParams()` returns the default of data-model.md: `invShutterTime = 1`,
  `projection = OPTIONS_PROJECTION_PERSPECTIVE`, `imagePlane = invImagePlane = 1`, `aperture = 0`, identity matrices, identity `relRotQ`, every flag false, `sampling` as `CShadingContext`'s default (`shading.h:894`);
  `installFrameParams(&fp)` then `installFrameParams(nullptr)` returns to the default; the accessor is never null.
- [X] T027 [P] [US2] Write `tests/unit/motion/test_motion_two_key.cpp` (`Motion_TwoKeyModel`, State-only; FR-011, decision 3, US2 AS2-AS3): over a dense set of finite keys and times including `t < 0` and `t > 1` (no clamp):
  `atTimeD(k0, k1, 0) == k0` and `atTimeD(k0, k1, 1) == k1` (compared with `==`; a `-0.0` key may come back `+0.0`); `atTimeF == atTime<float>`, `atTimeD == (float)atTime<double>`, `atTimeV` equals three `atTimeD`, bitwise;
  `atTimeD` and `atTimeV` equal verbatim copies of the quadric expression `(float)(p * (1.0 - t) + next * (double)t)` and of `interpolatev` (`mathSpec.h:126`), `atTimeF` equals a copy of `subdivision.cpp:186`'s expression, and the
  polygon fused form `atTime<double>` equals a copy of `polygons.cpp:312-351`'s; `keyAtTime(scratch, k0, nullptr, t) == k0` by **pointer identity** (a static object is returned unchanged); `sampleKey(PARAMETER_BEGIN_SAMPLE) == 0`,
  `sampleKey(PARAMETER_END_SAMPLE) == 1`; `toShutterRate(v)` equals `mulvf(v, invShutterTime)` for installed shutters `0 1`, `0 0.5`, `0.25 0.75`, `0.5 0.5`.
- [X] T028 [P] [US2] Write `tests/unit/motion/test_motion_isolation.cpp` (`Motion_Isolation`, State-only, narrow include path; FR-012, US2 AS5, SC-007): a test double supplies a `CRay`, a `CXform` and an installed `FrameParams`; `transformRay`,
  `toShutterRate` and `pluginTime` work with **no hider, no `CRenderer`, no shading context and no renderer-core object in the link line**. The target links `state_only_sources` and `libRIBCommon` only.
- [X] T029 [P] [US2] Write `tests/unit/motion/test_motion_camera_slerp.cpp` (`Motion_CameraSlerpEquivalence`, State-only; FR-021(a), research R8): for relative rotations (identity, 1e-4 rad, 5, 45, 90 degrees about each axis and an oblique axis, 179 degrees) and times
  (0, the strata 1/16 .. 15/16, 0.25, 0.5, 0.75, 1), compare **bitwise** (`memcmp`) `motion::cameraRotationAt(M, q, t)` against (1) Reyes's sequence `slerpq(Rjt, identQ, q, t); qtoR(M, Rjt)` (copies cited from `reyes.cpp:1433,1604,1643`,
  `stochasticQuad.h:520,884`, `rendererContext.cpp:740`) with `q = relRotQ`; (2) the conjugate forms of the pure-rotation fast path (`stochastic.cpp:166-221`) and of the ray transform (`objectMisc.h:209-214`), `q = conj(relRotQ)`.
  The Reyes copies are inline code that a test cannot call, and 028 edits no Reyes file (checked by T050), so a verbatim copy cited by file and line stays exact.
- [X] T030 [P] [US2] Write `tests/unit/motion/test_motion_transform_ray.cpp` (`Motion_TransformRayEquivalence`, State-only; FR-019): `motion::transformRay` against a verbatim copy of `objectMisc.h:178-232` (cited), `memcmp` on `oFrom` and `oDir`, for object
  motion (`xform->next` set, `cameraMotion` false), camera translation, camera rotation and rotation-only, over rays at times `-0.25 .. 1.25`; an object with its own transform motion under a camera that has rotation gets **no** camera rotation (GitHub #106 pinned).
- [X] T031 [P] [US2] Write `tests/unit/motion/test_frame_params_equal_core.cpp` (`Frame_ParamsEqualCore`, a Consumer linking `ribRender` and `openrendercommon`; FR-001, FR-002, decision 5, session reuse). For perspective and orthographic, DOF on and off, a crop window,
  camera translation, rotation and rotation-only, and the shutters `0 1`, `0 0.5`, `0.25 0.75`, `0.5 0.5`: **every field of the installed `FrameParams` equals the matching `CRenderer` static bitwise (`memcmp`), read from inside the frame** (for example from the capture context's
  `addObject`, which runs between `RiWorldBegin` and `RiWorldEnd`); `invShutterTime` equals the static reciprocal and is 1 for the empty shutter; no `CSamplingConfig` field is repeated outside `sampling`; after `RiWorldEnd` `frameParams()` is the default value again;
  a second `RiBegin`/`RiEnd` session installs a fresh value (gotcha 20). `EXCLUDE_FROM_ALL` + `DISABLED` until T045.
- [X] T032 Register the Phase 2 tests: `tests/unit/camera/CMakeLists.txt` (the 6 `camera`-label tests via `add_camera_motion_test`) and `tests/unit/motion/CMakeLists.txt` (the 4 State-only `motion` tests through the same helper when the option is ON, and
  `Frame_ParamsEqualCore` as a plain Consumer executable). All are `EXCLUDE_FROM_ALL` + `DISABLED`; `ctest -L '^camera$|^motion$'` selects them together with the Phase 1 replay tests.
- [X] T033 [RUN] Red evidence. (a) CP-a in `build-028` (option OFF: the State-only camera/motion targets do not exist, so archcheck is unaffected).
  (b) In a scratch build directory **inside the worktree** (`build-028-red`, deleted afterwards) configured like `build-028` plus `-DOPENRENDER_CAMERA_MOTION_ONLY_TESTS=ON`, run `cmake --build build-028-red --target <exe>` for each of the 10 State-only targets
  (only `openrendercommon` and the State objects need building there; never the whole project; do not run archcheck there). **Each must fail to compile**; record the first error per target (expected: `camera.h` / `frameParams.h` / `motion.h` not found, the quickstart §1 table).
  (c) In `build-028`: `cmake --build build-028 --target <Frame_ParamsEqualCore exe>` fails to compile (`frameParams.h`).
  (d) Disabled tests, two runs (the option is OFF in `build-028`, so the 10 State-only tests do not exist there and can be shown disabled only in `build-028-red`):
  (d1) `ctest --test-dir build-028 -L '^camera$|^motion$' --output-on-failure`: `Frame_ParamsEqualCore` reports "Not Run (Disabled)", the 8 replay tests pass, and **ctest's exit status is 0**.
  (d2) before deleting it, `ctest --test-dir build-028-red -R '^(Camera_|Frame_DefaultValue$|Motion_(TwoKeyModel|Isolation|CameraSlerpEquivalence|TransformRayEquivalence)$)'`: the 10 tests report "Not Run (Disabled)" and **the exit status is 0**.
  If either exit status counts a disabled test as a failure, stop and report rather than changing the convention.
  Delete `build-028-red`. Record all of this in validation.md §2.
- [X] T034 [US3] [RUN] CP-a, CP-b, CP-c on the tests-only tree (no `src/` file changed, so CP-d is not needed; the option is OFF): all green, `DB-004 x 65`, `DB-011 x 173`, PASS; record the new totals.
- [X] T035 **Checkpoint 2 (TEST-APPROVAL, PAUSE 2)**: stage the tests and CMake changes; summarize which tests are red by build failure, which are characterization tests and why, the nine mutations of T016, and the replay's per-family counts. **Wait for the maintainer's approval of the tests.** After approval the
  replay tool, `tests/unit/motion/replay/macos.txt` and the tests are frozen: any later edit to them needs the maintainer's explicit approval and a redo of T036 and T037.
- [X] T036 **Linux baseline handover (hard gate, decision Q8)**. After the approval of T035, and **before any phase-3 edit**, write the procedure into validation.md §3 and hand it to the coordinator, who runs it over ssh. The agent has no Linux access. It is the plan's quickstart §4a, **in
  this order** (the replay must exist before the full suite, or the eight `Motion_Replay_*` tests would fail on Linux with "no replay file"):
  - **Sync manifest**: stage the approved paths explicitly (`git add specs/028-camera-and-motion-layer tests/CMakeLists.txt tests/unit/state tests/unit/camera tests/unit/motion`), never `git add -A`, then `git ls-files -z | xargs -0 shasum -a 256 > $T/028-linux-sync.sha256` (tracked and staged files only; no `build*`, no `.git`). The coordinator syncs that file list to a **fresh remote directory**
    (`rsync --files-from`, quote hosts as `${H}:path`, memory `reference_zsh_host_colon_modifier.md`) and verifies with `sha256sum -c` on the remote.
  - **Remote steps** (remote `cpujob`, at `-j2`, `nice -n 19`; default options; Release; GCC 13; `-Werror`): (1) configure and build; (2) `ctest -L architecture`, and save the full `archcheck` report as `$T/archcheck-base-linux.txt`; (3) `ctest -R '^Motion_ReplayRecord$'`, copy `build/tests/unit/motion/replay-recorded-linux.txt` to
    `tests/unit/motion/replay/linux.txt` **in the remote tree**, and `ctest -R '^Motion_Replay_'` (8/8 green); (4) stamp, `ctest -LE determinism-sweep -j2`, then `image_manifest.sh record` into `$T/baseline-images-linux.sha256` and `diffs` into `$T/baseline-diffs-linux.txt`; (5) stamp, a **second** suite run, `diff` against the first manifest into
    `$T/baseline-image-noise-linux.txt`; (6) copy the four `tools/` files and the replay file back into the worktree.
  - **If GCC 13 `-Werror` rejects the new test code** (a Linux-only warning): the coordinator reports it; make the minimal fix, confirm on macOS that the replay is **byte-identical** to `replay/macos.txt` (re-run T014 on a scratch copy of the build tree, not into the tracked file) and that all tests pass, ask the maintainer to re-approve the changed test files, recompute the sync manifest and have only the changed files re-synced
    with checksums. If the Linux starting tree has a failure of its own, stop and report.
  - State in the handover that the Linux baseline is on the worktree **as it stands at the gate**: no `src/` change, `OPENRENDER_CAMERA_MOTION_ONLY_TESTS` OFF, and that from phase 3 on the tree can no longer build the starting `src/` with these tests, so it cannot be taken later.
- [X] T037 **Checkpoint 2b (LINUX GATE, PAUSE 2b)**: wait for the coordinator's report and for the files to be copied back. Then verify and record in validation.md §3:
  - the five files exist and are non-empty: `$T/archcheck-base-linux.txt`, `$T/baseline-images-linux.sha256`, `$T/baseline-diffs-linux.txt`, `$T/baseline-image-noise-linux.txt` (it may be empty or short; explain every entry), and `tests/unit/motion/replay/linux.txt` (its header line reads `# motion_replay v1 linux master-4eb683b`);
  - `archcheck-base-linux.txt` shows `RESULT PASS`; record its `ALLOWED` counts (DB-004, DB-011) and `METRIC` rows as the Linux baseline for T111, and note any platform-tagged difference from macOS;
  - the coordinator's log shows the eight `Motion_Replay_*` green on Linux, the architecture check PASS, and two suite runs; `git diff --stat 4eb683b -- src` is still empty (the baseline was taken on the starting `src/`);
  - the replay file was produced by `Motion_ReplayRecord`, not edited by hand.
  Stage the files. **Phase 3 does not start until the coordinator confirms the gate is released.** If the Linux baseline cannot be taken, stop and report: the alternative (rebuilding it later from master plus a named subset of 028's test files) is the maintainer's decision.

---

## Phase 3: Foundation (US1, US2, US5; FR-001 to FR-012, FR-019)

**Goal**: `FrameParams`, the camera, the motion evaluator and the core's builder exist, are installed per frame and are covered by their unit tests, with **no consumer yet**.

**Independent test**: all 11 unit tests are green; every render is byte-identical (nothing reads the new values until Phase 4).

- [X] T038 [US2] Create `src/ri/state/frameParams.h` and `frameParams.cpp` per contracts/frame-params.md and data-model.md: `FrameCameraMotion`, `FrameParams` (embeds `sampling::CSamplingConfig sampling` **by value** and repeats **none** of its fields: FR-002), `installFrameParams(const FrameParams *)`, inline `frameParams()` over a pointer defined in the `.cpp`, and the default value.
  - **Constraints quoted from data-model.md**: every field equals the `CRenderer` static it is copied from, bit for bit; `invShutterTime` is the stored reciprocal, never recomputed from `shutterTime` by a reader (decision 5); default value (nothing installed): `invShutterTime = 1`, `projection = OPTIONS_PROJECTION_PERSPECTIVE`,
    `imagePlane = invImagePlane = 1`, `aperture = 0`, identity matrices, identity `relRotQ`, every flag false, `sampling` as `CShadingContext`'s default (`shading.h:894`, reproduced without including it).
  - The **header** includes only `common/algebra.h` and `common/sampling.h` (Infra); **never** `renderer.h` or `rendererContext.h` (FR-003). `projection` is a plain `int`, so the header does not name the enum. `OPTIONS_PROJECTION_PERSPECTIVE` is defined in `src/ri/state/options.h` (State):
    `frameParams.cpp` (and `camera.cpp`, T039) include it for the default value and the projection test, with the `static_assert` on the two enumerators of contracts/frame-params.md; if that include drags in a header outside State and `common/`, stop and report. Plain aggregate, trivially copyable, no pointer, no method beyond `const` accessors.
  - **The default value is intended.** A read outside a frame returns defined values instead of dereferencing null; the price is that a read made before `beginFrame` would get the default silently. That is accepted by design: `Frame_DefaultValue` pins the value, and **CP-b is the guard** (every replay test and the whole suite read inside the frame, so a migrated site that read the default would turn them red).
  - **Comments must not contain the qualified name `CRenderer::invShutterTime`** (here, in `frameParams.cpp`, or in any new file but `rendererFrame.cpp`'s copy statement): SC-001 and T099 count files by that qualified symbol (9 before, 3 after), and a comment would add a fourth. Write "the renderer core's shutter reciprocal" or the unqualified field name `invShutterTime`.
- [X] T039 [US1] Create `src/ri/state/camera.h`, interface part: `CameraSample`, `CameraRay`, `LensModel` (`thinLens()`, `cocFactorScreen()` with `renderer.cpp:447`'s expression, 0 for a pinhole), `enum class CameraSupport { Supported, NotSupported }`, **forward declarations only** of `CameraImportance` and `CameraConnectionSample` (D18: their definition belongs to the BDPT spec), and `ICamera` per contracts/camera.md.
  - `generateRay` is pure virtual; `sampleRay(const CameraSample &)` is a **non-virtual** inline forwarder (§7.4's name); `lensModel()`, `evaluateImportance` and `sampleConnection` are virtual with default bodies (pinhole; `NotSupported`, outputs untouched). Keep `generateRay` first in the vtable and do not reorder later (FR-009).
  - The header includes only `frameParams.h` and `common/` headers. Create `camera.cpp` with the default bodies; it also includes `state/options.h` for the projection enumerators (T038).
- [X] T040 [US1] In `camera.h`/`camera.cpp`: `CFrameCamera final : ICamera`, `explicit CFrameCamera(const FrameParams &)`, holding a copy of the camera fields it reads; `generateRay` and `lensModel` overrides, every method `const`, no random state, no allocation.
  - **Reproduce `CRaytracer::computeSamples` (`raytracer.cpp:626-701`) and `fullImagePixels2camera()` (`renderer.h:618-631`) expression by expression** (research R9; contracts/camera.md table): `x' = x * dxdPixel + sampling.screenLeft`, `y' = y * dydPixel + sampling.screenTop`; perspective `(x' * z * invImagePlane, y' * z * invImagePlane, z)`, orthographic `(x', y', z)`; pinhole aim at `z = imagePlane`, thin lens `from[X] += lensU * aperture`
    (compound assignment, as today) and aim at `z = focaldistance`; `dir = normalizev(subvv(aim, from))`; `time = t`; `da = dxdPixel / imagePlane` (perspective) or `db = dxdPixel` (orthographic). Same operand order, same `mathSpec.h` helpers.
  - **Crop invariance**: the mapping uses the screen window's own origin (`fp.sampling.screenLeft/Top`), never the rendered window's.
- [X] T041 [US2] Create `src/ri/geometry/motion.h` (header-only, inline, namespace `openrender::motion`), part 1 per contracts/motion-evaluator.md: `template <class S> inline S atTime(S key0, S key1, S t) { return key0 * (S(1) - t) + key1 * t; }` (**the only definition of the model; this operand order is the shape of every starting-tree site**), `atTimeD`, `atTimeF`, `atTimeV`, `keyAtTime`, `sampleKey`, `shutterRate()`
  (`frameParams().invShutterTime`), `toShutterRate(float *)` (`mulvf(v, shutterRate())`), `pluginTime(const CRay &)`.
  - Includes: `common/algebra.h`, `common/mathSpec.h`, `common/rendererc.h` (Infra; defines `PARAMETER_BEGIN_SAMPLE` and `PARAMETER_END_SAMPLE`, used by `sampleKey`), `ray.h` (`CRay`), `xform.h` and `frameParams.h`. If `ray.h`, `xform.h` or `rendererc.h` pulls a header the narrow path of `Motion_Isolation` cannot resolve, stop and report. **Never** `renderer.h`, `rendererContext.h`, `shading.h` or a hider header (FR-012). Unclamped: `t` outside 0..1 extrapolates as today.
  - **Comments must not contain the qualified name `CRenderer::invShutterTime`** (SC-001 counts files by it, T099): refer to `frameParams().invShutterTime` or "the shutter reciprocal".
- [X] T042 [US2] `motion.h`, part 2 (verbatim moves; the originals stay in `objectMisc.h` until Phases 5, 6 and 11 delete them): `transformAtTime`, `vtransformAtTime`, `ntransformAtTime` (from `objectMisc.h:66-139`), `cameraRotationAt(float *M, const float *q, float t)` (`slerpq(identity, q, t)` then `qtoR`), and
  `transformRay(float *oFrom, float *oDir, const CXform *xform, const CRay *ray)` (from `objectMisc.h:178-232`), reading `cameraHasRotation`, `relRotQ` and `toWorld` from `frameParams().camera`. It composes only what `CRendererContext::addObject` synthesizes, so an object with its own transform motion gets no camera rotation (#106 preserved, FR-021(b)).
  Do not edit `objectMisc.h` in this phase.
- [X] T043 [US2] Create `src/ri/render/rendererFrame.h` and `rendererFrame.cpp` (renderer core): `beginFrameParams(const sampling::CSamplingConfig &)` copies every field from the `CRenderer` statics named in data-model.md (it **computes nothing and derives nothing**), installs the value with `installFrameParams` and builds the `CFrameCamera`;
  `endFrameParams()` uninstalls and deletes the camera; `const ICamera &frameCamera()`. The value and the camera live in the file for the length of the frame. A core file may read `CRenderer` freely. The qualified name `CRenderer::invShutterTime` appears in `rendererFrame.cpp` only (the third file of SC-001's count), never in `rendererFrame.h` or in a comment elsewhere (T038).
- [X] T044 [US2] Wire the core: in `src/ri/render/renderer.cpp` call `beginFrameParams(samplingConfig)` right after the `CSamplingConfig` is built (`:775`) and **before the contexts are created**, and `endFrameParams()` in `endFrame` **after the contexts are deleted**; in `src/ri/CMakeLists.txt` add `state/frameParams.cpp`, `state/camera.cpp` and `render/rendererFrame.cpp`
  to `ri_sources`, and add `render/rendererFrame.cpp` to the `list(REMOVE_ITEM ribVector_sources ...)` beside `render/renderer.cpp` (ribVector excludes it as it excludes `renderer.cpp`: no new dispatch pair, DB-017). Confirm `ribVector`, `ribOut`, the preview and the Python/Lua consumers still configure. Do not edit `renderer.h`, `rendererStatics.cpp` or `rendererContext.cpp`.
- [X] T045 [US1] Enable the tests. In `tests/unit/state/CMakeLists.txt` change the option's default to **ON** (the `cmake_dependent_option` of T020 keeps it OFF whenever `OPENRENDER_STATE_ONLY_TESTS` is OFF) and **remove the `if(EXISTS ...)` guards** (the two sources join `state_only_sources` unconditionally); remove `EXCLUDE_FROM_ALL` and `DISABLED` from the 10 State-only camera/motion tests and from `Frame_ParamsEqualCore`. Reconfigure `build-028` with `-DOPENRENDER_CAMERA_MOTION_ONLY_TESTS=ON`
  (the cached value is OFF and a changed default does not override a cache entry).
- [X] T046 [RUN] Build (CP-a) and run `$CJ 028-p3-motion ctest --test-dir $W/build-028 -L '^motion$|^camera$' --output-on-failure`: all 11 new tests and the 8 replay tests **green**; record that T033's compile errors are gone. A new test that fails is a bug in the code first; if the test itself is wrong, **stop and report** (the tests are approved and frozen).
- [X] T047 [US1] [RUN] Mutation checks, camera (stage the files first; each: scratch edit, rebuild, the named test red, revert, `git diff -- <file>` empty): (1) `Camera_Analytic`: flip the sign of the lens offset in `CFrameCamera::generateRay`; (2) `Camera_StartingTreeEquivalence`: compute `dir = normalizev(subvv(from, aim))` (reversed operands) and show it red;
  (3) `Camera_CropInvariance`: read `pixelLeft`-style crop origin (`sampling.originX`) instead of `screenLeft`; (4) `Camera_GenerateRayOnly`: make the default `evaluateImportance` return `Supported`; (5) `Camera_LensModel`: change `cocFactorScreen()`'s expression; (6) `Frame_DefaultValue`: set the default `invShutterTime` to 2. Record in validation.md §4.
- [X] T048 [US2] [RUN] Mutation checks, motion (same procedure): (1) `Motion_TwoKeyModel`: change `atTime`'s operand order to `key1 * t + key0 * (S(1) - t)` (or drop the `(double)` evaluation in `atTimeD`); (2) `Motion_Isolation`: add `#include "renderer.h"` to `motion.h` and show the target fails to compile or link (the isolation test goes red); (3) `Motion_CameraSlerpEquivalence`: drop the conjugate in
  `cameraRotationAt`'s caller path or change `slerpq`'s argument order; (4) `Motion_TransformRayEquivalence`: apply camera rotation when `xform->cameraMotion` is false (the #106 case). Revert each. Record in validation.md §4.
- [X] T049 [US2] [RUN] Mutation check, core: (1) `Frame_ParamsEqualCore`: in `rendererFrame.cpp` copy `shutterTime` into the `invShutterTime` field (decision 5) and show it red; (2) show it red when `endFrameParams()` does not uninstall (default value not restored). Revert. Record in validation.md §4.
- [X] T050 [US2] Greps and diffs (quickstart §3), results into validation.md §4: `grep -nE '#include *"(.*/)?(renderer|rendererContext)\.h"' src/ri/state/frameParams.* src/ri/state/camera.* src/ri/geometry/motion.h` prints nothing (FR-003); `git diff --stat 4eb683b --` the "Files 028 never edits" list is empty (for `renderer.h`, `rendererStatics.cpp`, `rendererContext.cpp`, `shading.*`, `texture/`, the Reyes files, `sampler.*`, `object.cpp`, `csgDefinition.cpp`, `implicit.h`, `dlo.h`); `git diff 4eb683b -- src/ri/geometry/surface.cpp` is empty in this phase. Also confirm that the Reyes SLERP call shapes `Motion_CameraSlerpEquivalence` copies (T029) are still present at the cited places, since the verbatim copies are only as good as the code they cite:
  `grep -nE 'slerpq\(Rjt(_pix)?, identQ, ' src/ri/hiders/reyes.cpp src/ri/hiders/stochastic.cpp src/ri/hiders/stochasticQuad.h src/ri/render/rendererContext.cpp src/ri/geometry/objectMisc.h` lists the eight sites
  (`reyes.cpp` 1433, 1604, 1643; `stochasticQuad.h` 520, 884; `rendererContext.cpp` 740; `stochastic.cpp` 220, conjugate `invRotQ`; `objectMisc.h` 213, conjugate `relRotQ_inv`), and a site that moved is re-read and the copy in the test updated **only with the maintainer's approval** (the tests are frozen).
- [X] T051 [US3] [RUN] **Checkpoint 3 (PAUSE 3)**: CP-a, CP-b, CP-c, **CP-d** (every image identical: nothing reads the new values yet), CP-m. Expected: all 11 new unit tests green; `ALLOWED DB-004 x 65`, `DB-011 x 173`, metrics Geometry 78/19, Hiders 567/84, TOTAL 721/110; the new State/Geometry/core files add allowed edges only and no unclassified file appears. Record in validation.md §1; stage; summarize; wait for review.

---

## Phase 4: US1, the camera (FR-004 to FR-008; SC-002)

**Goal**: `CRaytracer::computeSamples` generates every primary ray through `ICamera` and reads no camera static.

**Independent test**: `grep` for the seven statics and `fullImagePixels2camera` in `raytracer.cpp` prints nothing; every raytrace image is byte-identical (CP-d over the full suite); the interleaved SC-008 timing is within the starting tree's variance.

- [X] T052 [US4] [RUN] Delete the 7 DB-011 lines of contracts/allowlist-end-state.md ("Phase 4 (camera)"), keyed by `src/ri/hiders/raytracer.cpp -> ` `_ZN9CRenderer10imagePlaneE`, `10projectionE`, `13focaldistanceE`, `13invImagePlaneE`, `8apertureE`, `8dxdPixelE`, `8dydPixelE`; add and swap nothing. `ctest -L architecture` must be **red with exactly those 7 `VIOLATION` lines** and nothing else (T-A1); record them in validation.md §5. Any other violation: stop.
- [X] T053 [US1] `src/ri/hiders/raytracer.h` and `raytracer.cpp`: the constructor becomes `CRaytracer(int thread, const ICamera &camera)` (definition at `raytracer.cpp:390`), holding the reference; include `camera.h`. `computeSamples` (`:626-700`) calls `camera.generateRay(cRay->xAbs, cRay->yAbs, cRay->time, lens)` per ray and copies `from`, `dir`, `da`, `db` into the `CPrimaryRay`; `sample()` is unchanged
  (the film mapping to `xAbs`/`yAbs`, the time and `lensU`/`lensV` from `CSampler::cameraSample()`: FR-005); `t`, `flags`, `tmin` and `depthProbe` are set as today; remove locals that became unused. Move the comment at `:677-681` (camera motion is applied by `motion::transformRay`, not by the camera) with the behaviour it describes.
  No `CRenderer::` projection, image-plane, aperture, focal-distance or pixel-size read and no `fullImagePixels2camera` call may remain in this file (US1 AS5, FR-008). If another function in `raytracer.cpp` still reads one of the seven statics, stop and report (the allowlist deletion assumed they all go).
- [X] T054 [US1] `src/ri/render/renderer.cpp:787`: `contexts[i] = new CRaytracer(i, frameCamera());`. `grep -rn 'CRaytracer(' src tests` shows no other construction site (the Python/Lua bindings and tests included); if one exists, update it and record it.
- [X] T055 [US3] [RUN] CP-a, CP-b, CP-c, **CP-d**, CP-m. Expected: the 7 deleted DB-011 lines are gone with **no `STALE` and no `VIOLATION`**; `ALLOWED DB-004 x 65`, `DB-011 x 166`; `grep -nE 'CRenderer::(imagePlane|invImagePlane|projection|focaldistance|aperture|dxdPixel|dydPixel)|fullImagePixels2camera' src/ri/hiders/raytracer.cpp` prints **nothing**; every raytrace scene's image is byte-identical (the camera is on every
  raytrace ray, so the whole suite is the guard, not only the camera scenes). Record in validation.md §1 and §5.
- [X] T056 [US3] [RUN] SC-008 timing, **interleaved** (validation.md §0.5; the method spec 039's SC-010 used): for each of `Visual_teapot-motion-raytrace`, `Convergence_dof-raytrace` and `Visual_blobby-motion-grow-raytrace` run **five alternating pairs**, a starting-tree run in `$X/bld` then a current-tree run in `build-028`, each `$CJ 028-p4-time ctest --test-dir <tree> -R '^<test>$'` alone;
  compare the medians of the pairs. The current tree's median must not exceed the starting tree's by more than the starting tree's own spread (a plain before/after band is unreliable for the teapot scene: wall time follows machine load and thermals). Record every run. A regression beyond the spread stops the phase (the camera call is accepted only if SC-008 holds).
- [X] T057 **Checkpoint 4 (PAUSE 4)**: stage; record the CP results, the 7 `VIOLATION` lines of T052 and the timing table; summarize (US1 done: the MVP of the spec); wait for review.

---

## Phase 5: The ray transform shared by `CDLObject` and the quadrics (US2; FR-013, FR-019, FR-021)

**Goal**: the ray-side camera-motion transform, with its SLERP, is `motion::transformRay`; `objectMisc.h`'s copy is gone.

**Independent test**: `Motion_Replay_raytransform` and `Motion_TransformRayEquivalence` green; the 6 camera-state DB-004 lines are gone.

- [X] T058 [US4] [RUN] Delete the 6 DB-004 lines ("Phase 5 (ray transform)"), keyed by `src/ri/geometry/dlobject.cpp` and `src/ri/geometry/quadrics.cpp -> ` `_ZN9CRenderer17cameraHasRotationE`, `_ZN9CRenderer7relRotQE`, `_ZN9CRenderer7toWorldE`. `ctest -L architecture`: **exactly 6 `VIOLATION` lines**, all in those two files. Record them in validation.md §5.
- [X] T059 [US2] Replace the 8 call sites of the ray `transform(oFrom, oDir, xform, <ray>)` with `motion::transformRay(oFrom, oDir, xform, <ray>)`: `dlobject.cpp:99` and the seven quadrics in `quadrics.cpp` (`:197, 687, 1057, 1500, 1956, 2402, 2957`; re-read the lines, the file is large); include `motion.h`. Then delete the ray `transform()` (`objectMisc.h:178-232` only) from `objectMisc.h`
  after `grep -n 'transform(oFrom' src` shows no remaining caller. **Leave the grid `transform/vtransform/ntransform` overloads (`objectMisc.h:66-139`) alone**: Phase 6 owns them.
- [X] T060 [US2] [RUN] CP-a, CP-b, CP-c, **CP-d**, CP-m (`Motion_Replay_*` all 8 green, including the quadrics scenes under object transform motion). Expected: the 6 lines gone, **no `STALE`, no `VIOLATION`**, `ALLOWED DB-004 x 59`, `DB-011 x 166`.
  `grep -n 'cameraHasRotation\|relRotQ\|toWorld' src/ri/geometry/*.h src/ri/geometry/*.cpp` prints only `motion.h` (reading `frameParams().camera`); any other hit is listed in validation.md with its file and why it is not one of the 6 edges.
- [X] T061 **Checkpoint 5 (PAUSE 5)**: stage; summarize; wait for review.

---

## Phase 6: Quadrics (US2; FR-013, FR-016, FR-018; `quadrics.cpp`)

**Goal**: the seven quadrics take their parameter interpolation, their grid transforms at time and the `dPdtime` scale from the evaluator, in both `intersect()` and `sample()`.

**Independent test**: `Motion_Replay_quadrics` green; `grep -c 'CRenderer::invShutterTime' src/ri/geometry/quadrics.cpp` is 0.

**Per-site rule for the seven quadric tasks** (research R9, FR-014): keep each site's **exact expression shape** and its own precision. The intersect-side line `(float)(p * (1.0 - rv->time) + nextData[k] * (double)rv->time)` becomes `motion::atTimeD(p, nextData[k], rv->time)`; the `sample()` side uses `ctime` the same way; each `dPdtime` read
`mulvf(dest, CRenderer::invShutterTime)` becomes `motion::toShutterRate(dest)`. A site whose shape differs from this is stopped and reported, not migrated by guess. Each quadric has 2 `invShutterTime` reads (14 in all).

- [X] T062 [US4] [RUN] Delete the 1 DB-004 line `src/ri/geometry/quadrics.cpp -> _ZN9CRenderer14invShutterTimeE`. `ctest -L architecture`: **exactly 1 `VIOLATION`**. Record it.
- [X] T063 [US2] `quadrics.cpp:48-80` (the `transformPoints()` macro): `transform`/`vtransform`/`ntransform` with `time` become `motion::transformAtTime`/`vtransformAtTime`/`ntransformAtTime`; include `motion.h` if not already.
- [X] T064 [US2] `CSphere`: intersect-side parameters (`~:200-203`), `sample()` parameters (`~:327-331`) and its two `dPdtime` scales, per the rule above.
- [X] T065 [US2] `CDisk`: intersect parameters, `sample()` parameters, two `dPdtime` scales.
- [X] T066 [US2] `CCone`: intersect parameters, `sample()` parameters, two `dPdtime` scales.
- [X] T067 [US2] `CParaboloid`: intersect parameters, `sample()` parameters, two `dPdtime` scales.
- [X] T068 [US2] `CCylinder`: intersect parameters, `sample()` parameters, two `dPdtime` scales.
- [X] T069 [US2] `CHyperboloid`: intersect parameters, `sample()` parameters, two `dPdtime` scales.
- [X] T070 [US2] `CToroid`: intersect parameters, `sample()` parameters, two `dPdtime` scales.
- [X] T071 [US2] Delete the grid `transform`, `vtransform` and `ntransform` overloads with `time` from `objectMisc.h:66-139` **only if** `grep -rn` shows no remaining caller in `src` (the quadrics were the users); if a caller remains, leave the overload and record the file in validation.md for Phase 11.
- [X] T072 [US2] [RUN] CP-a, CP-b, CP-c, **CP-d**, CP-m. Expected: **no `STALE`, no `VIOLATION`**, `ALLOWED DB-004 x 58`, `DB-011 x 166`; `grep -c 'CRenderer::invShutterTime' src/ri/geometry/quadrics.cpp` prints 0; `Motion_Replay_quadrics` green. (A single-object compile of `quadrics.cpp` through `cpujob`, with the coordinator's approval, is allowed after any quadric task to localize an error.)
- [X] T073 **Checkpoint 6 (PAUSE 6)**: stage; summarize; wait for review.

---

## Phase 7: Patches (US2; FR-013, FR-016, FR-018; `patches.cpp`)

**Goal**: `CBilinearPatch::intersect` and the three `sample()` in `patches.cpp` take their key interpolation and `dPdtime` scale from the evaluator.

**Independent test**: `Motion_Replay_bilinear` and `Motion_Replay_tessellation` (sample side) green; `patches.cpp` reads no `CRenderer::invShutterTime`.

- [X] T074 [US4] [RUN] Delete the 1 DB-004 line `src/ri/geometry/patches.cpp -> _ZN9CRenderer14invShutterTimeE`. `ctest -L architecture`: **exactly 1 `VIOLATION`**. Record it.
- [X] T075 [US2] `CBilinearPatch::intersect` (`patches.cpp:228-234`): `interpolatev(t0, P00, P00 + 4 * vertexSize, time)` and the three sibling calls become `motion::atTimeV` (same key addresses, same order).
- [X] T076 [US2] The three `sample()` key lerps (`patches.cpp:372-374`, `692-695`, `1022-1024`; the bilinear, bicubic and NURBS patches) become `motion::atTimeD`, keeping each family's own `moving` test and its key selection (`sampleKey` only where the site selects begin or end). **The NURBS patch's lerp (`:1022`) has `double` keys and stores into a `double *`: it calls `(float)motion::atTime<double>(...)` instead of `atTimeD`, whose `float` parameters would round the keys (found by `Motion_Replay_tessellation`).**
- [X] T077 [US2] The three `dPdtime` scales (`patches.cpp:481, 793, 1311`) become `motion::toShutterRate(dest)`. **`CBilinearPatch::sample()`'s precedence defect (`patches.cpp:477`: `a - b * (1 - cv) + c * cv` where `a - (b * (1 - cv) + c * cv)` is meant, GitHub #108) is preserved bit for bit: do not add parentheses.** Add a one-line comment citing #108 beside the expression and change no other character of it.
- [X] T078 [US2] [RUN] CP-a, CP-b, CP-c, **CP-d**, CP-m. Expected: **no `STALE`, no `VIOLATION`**, `ALLOWED DB-004 x 57`; `grep -c 'CRenderer::invShutterTime' src/ri/geometry/patches.cpp` prints 0; `Motion_Replay_bilinear` and `_tessellation` green (the `S` records of the bicubic and NURBS scenes pin the three sample paths and the #108 expression).
- [X] T079 **Checkpoint 7 (PAUSE 7)**: stage; summarize; wait for review.

---

## Phase 8: Polygons (US2; FR-013, FR-016, FR-018; `polygons.cpp`)

**Goal**: `CPolygonTriangle` and `CPolygonQuad` take their intersection interpolation, their fused `sample()` lerps and their `dPdtime` scale from the evaluator.

**Independent test**: `Motion_Replay_polygons` green; `polygons.cpp` reads no `CRenderer::invShutterTime`.

- [X] T080 [US4] [RUN] Delete the 1 DB-004 line `src/ri/geometry/polygons.cpp -> _ZN9CRenderer14invShutterTimeE`. `ctest -L architecture`: **exactly 1 `VIOLATION`**. Record it.
- [X] T081 [US2] Triangle and quad intersection (`polygons.cpp:183-187`, `730-736`): `interpolatev` becomes `motion::atTimeV`.
- [X] T082 [US2] The fused `sample()` lerps (`polygons.cpp:309-372`, `879-922`): the bilinear and temporal interpolation are **one double expression** today; the site computes its two spatial operands exactly as now and applies the model through `motion::atTime<double>(spatial0, spatial1, ctime)` cast to float (contracts/motion-evaluator.md); the normal's `interpolatev` becomes `atTimeV`. Do not split a statement or reorder an operand (FMA contraction, research R9). Hoisting the two spatial operands into named locals is **not needed**: the outer `a * (1 - t) + b * t` is the same single expression inside `atTime`, and contraction is decided per expression, so the choice is the same as the fused site's; the replay's `S` records and CP-d are the guard.
- [X] T083 [US2] The two `dPdtime` scales (`polygons.cpp:482`, `1046`) become `motion::toShutterRate(dest)`.
- [X] T084 [US2] [RUN] CP-a, CP-b, CP-c, **CP-d**, CP-m. Expected: **no `STALE`, no `VIOLATION`**, `ALLOWED DB-004 x 56`; `grep -c 'CRenderer::invShutterTime' src/ri/geometry/polygons.cpp` prints 0; `Motion_Replay_polygons` green.
- [X] T085 **Checkpoint 8 (PAUSE 8)**: stage; summarize; wait for review.

---

## Phase 9: Implicit surface (US2; FR-013; `implicitSurface.cpp`)

**Goal**: the implicit surface's plug-in time goes through one documented evaluator entry point. No allowlist line is deleted (research R11).

**Independent test**: `Motion_Replay_implicit` green (the first coverage of `CImplicit`).

- [X] T086 [US2] `implicitSurface.cpp:142, 155, 176`: `ray->time` passed to `evalFunction`/`evalNormalFunction` becomes `motion::pluginTime(*ray)` (the unclamped shutter fraction, the value the ray carries). No arithmetic moves; **`implicit.h` (the Procedures ABI) is unchanged** and its DB-003 entry is untouched.
- [X] T087 [US2] [RUN] CP-a, CP-b, CP-c, **CP-d**, CP-m. Expected: **no allowlist change**, `ALLOWED DB-004 x 56`, `DB-011 x 166`; `git diff 4eb683b -- src/ri/geometry/implicit.h` is empty; `Motion_Replay_implicit` green.
- [X] T088 **Checkpoint 9 (PAUSE 9)**: stage; summarize; wait for review.

---

## Phase 10: Tessellation (US2; FR-013; `surface.cpp`, the moving branch only)

**Goal**: `CTesselationPatch::intersect`'s moving branch interpolates its two keys through the evaluator. No allowlist line is deleted (`surface.cpp`'s DB-004 lines are spec 031's).

**Independent test**: `Motion_Replay_tessellation` green (intersection and sample records).

- [X] T089 [US2] `src/ri/geometry/surface.cpp`, `CTesselationPatch::intersect` moving branch (`:1453-1600`): `interpolatev(P.., cP0, cP1, timev)` becomes `motion::atTimeV` (include `motion.h`). **Nothing else in `surface.cpp` changes** (the dicing probes, `timev = 1`, are spec 031's: FR-018).
- [X] T090 [US2] [RUN] CP-a, CP-b, CP-c, **CP-d**, CP-m. Expected: **no allowlist change**, `ALLOWED DB-004 x 56`; `git diff 4eb683b -- src/ri/geometry/surface.cpp | grep '^@@'` shows hunks only inside `CTesselationPatch::intersect` (quickstart §3); `Motion_Replay_tessellation` green.
- [X] T091 **Checkpoint 10 (PAUSE 10)**: stage; summarize; wait for review.

---

## Phase 11: Grid-only families, subdivision, dead helpers (US2, US4; FR-013, FR-016, FR-018, SC-001)

**Goal**: `curves`, `points`, `patchgrid` and `bsplinePatchgrid` (grid sampling only, no intersection) and `subdivision.cpp`'s live float interpolation (decision Q2) take their time handling from the evaluator, so that **no Geometry file reads `CRenderer::invShutterTime`**.

**Independent test**: `Motion_Replay_gridonly` and `_emptyshutter` green; SC-001 greps.

- [X] T092 [US4] [RUN] Delete the 4 DB-004 lines `src/ri/geometry/curves.cpp`, `points.cpp`, `patchgrid.cpp` and `bsplinePatchgrid.cpp` `-> _ZN9CRenderer14invShutterTimeE`. `ctest -L architecture`: **exactly 4 `VIOLATION` lines**. Record them.
- [X] T093 [US2] `curves.cpp` (`:385`, `:517`): the `dPdtime` scale via `motion::toShutterRate`; the key selection via `motion::sampleKey` where the site writes one (`:239`, `:247`, `:457`). **No lerp**: the curves keep no time-interpolation expression (grep confirms), so `atTimeD` is not used here.
- [X] T094 [P] [US2] `points.cpp` (`:241`): the `dPdtime` scale via `motion::toShutterRate`; key selection via `motion::sampleKey` where written (`:210`). **No lerp**, as in T093.
- [X] T095 [P] [US2] `patchgrid.cpp`: the `dPdtime` scale (`:380`) via `toShutterRate`; the key lerp (`:243-246`, key grids at `vertex + vertexSize * grid`) via `motion::atTimeD(vertex0[j], vertex1[j], ctime)`. Here `ctime` is `const double` initialised from `*time++` (a float), so it holds a float exactly and `atTimeD`'s `float t` narrows it without loss; check the initialiser when migrating, and stop if it is not a float value.
- [X] T096 [P] [US2] `bsplinePatchgrid.cpp`: the `dPdtime` scale (`:374`) via `toShutterRate`. **The key lerp (`:233-236`) stays in place** (maintainer decision, 2026-10-27, Checkpoint 1): it is a mixed-precision shape (`ctime` is `const float`, so in `vertex0[j] * (1.0 - ctime) + vertex1[j] * ctime` the left product is double but the right product is float times float, rounded to float before the add) that neither `atTimeD` (both double) nor `atTimeF` (both float) reproduces, and no entry point is added in 028.
  The file keeps its `ctime` hit in T099's grep, explained in validation.md as "left in place, mixed precision, for a later precision-unification phase" (the DB-004 line still goes: only the `invShutterTime` read is needed). The `subdiv-cube` S records (which contain `CBSplinePatchGrid` leaves) are the guard.
- [X] T097 [US2] `subdivision.cpp:176-191` (decision Q2; no `invShutterTime` read): `vertex0[j] * (1 - ctime) + vertex1[j] * ctime` with `const float ctime` becomes `motion::atTimeF(vertex0[j], vertex1[j], ctime)`. It stays **float-evaluated**: `atTimeD` here would change the rounding.
- [X] T098 [US2] Delete the dead helpers `interpolate()` and `interpolate3()` from `objectMisc.h` (`:145-172`) after `grep -rn` proves no caller exists (research R14 finding 1), and delete any grid-transform overload T071 left behind if its last caller is now gone.
- [X] T099 [US4] [RUN] CP-a, CP-b, CP-c, **CP-d**, CP-m. Expected: **no `STALE`, no `VIOLATION`**, `ALLOWED DB-004 x 52`, `DB-011 x 166`; all 8 `Motion_Replay_*` green. SC-001 and US2 AS4 greps, recorded in validation.md:
  - `grep -rn 'CRenderer::invShutterTime' src/ri/geometry src/ri/hiders` prints **nothing**;
  - **SC-001's file count** (maintainer decision 2026-10-27: counted by the qualified symbol; 9 before, 3 after). Record the output of `grep -rln 'CRenderer::invShutterTime' src`: baseline 9 (the 7 Geometry files, `rendererServicesImpl.h`, `rendererStatics.cpp`); expected now exactly 3: `rendererServicesImpl.h`, `rendererStatics.cpp` and `rendererFrame.cpp`. Any other file is a failure. Also record `grep -rln invShutterTime src` for information
    (the new `FrameParams`, `motion.h` and camera files carry the field name unqualified, decision 5; they are not counted, and the plain-pattern figure of 13 is historical).
  - `grep -nE '\(double\)[a-z]+->time|1\.0 - [a-z]+->time|ctime|interpolatev\(.*time|timev' src/ri/geometry/*.cpp` lists no remaining private interpolation in the 12 intersection entry points or `CDLObject` (US2 AS4); any hit is explained in validation.md (a hit in a file this spec does not own, such as the dicing probes, is expected and listed).
- [X] T100 [US3] [RUN] SC-008 timing, interleaved again, exactly as T056, against the same `$X/bld` starting-tree build: the three scenes, five alternating pairs each. Record every run and the verdict.
- [X] T101 **Checkpoint 11 (PAUSE 11)**: stage; record the SC-001 file list and the timing table; summarize (US2 and US4's code are complete); wait for review.

---

## Phase 12: Guardrails, documentation, build options and Linux (US4; FR-022, FR-023, FR-025, FR-026)

**Purpose**: the registry and allowlist counts, the docs, the JIT-off build, the finished-tree validation on both platforms and the single determinism sweep. Do not refer to the merge in any text.

- [ ] T102 [US4] Allowlist and metrics (FR-022, SC-009). Run the architecture report once more and: (a) `git diff 4eb683b -- tests/architecture/allowlist.txt | grep -c '^+[A-Z]'` is **0** and the deleted lines are exactly the 20 of contracts/allowlist-end-state.md; (b) update the allowlist header counts to the check's report (`DB-004 (65)` to `(52)`; `DB-011 (182)` to **`(168)`**: the header was stale, the file held 175 DB-011 lines at `4eb683b` and holds 168 after the 7 deletions, contracts/allowlist-end-state.md);
  (c) lower `tests/architecture/metrics-baseline.txt` to **exactly** the values the check prints (`NOTE metric ... < baseline`), never raising any class (predictions 48/15, 558/82, 682/107 are not authoritative); (d) no `STALE` line remains. Record the report in validation.md §1.
- [ ] T103 [P] [US4] Update `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` (FR-023, contracts/allowlist-end-state.md "Registry text"). `<date>` is the date this task is done.
  - **DB-009 stays Open.** Narrow its evidence to what remains: Reyes's circle-of-confusion model (`CSampler::circleOfConfusion`, `sampler.cpp:23`, 7 call sites in `reyes.cpp`) against the camera layer's thin lens, and Reyes's own motion and camera-rotation code (`reyes.cpp`, `stochastic.cpp`, `stochasticQuad.h`), with the line references refreshed (the old `stochastic.h:81,108`
    and `raytracer.cpp:612-621` citations have drifted); remove the `invShutterTime` bullet. Tracking stays on spec 031 (#37). History bullet: "spec 028, <date>: the raytrace side moved onto `ICamera`, `FrameParams` and `motion.h`; Reyes's side is spec 031". **Never "Resolved".**
  - **DB-004**: evidence drops `invShutterTime` (24 reads), `relRotQ` (4), `toWorld` and `cameraHasRotation`; "Measured by the architecture check" becomes 52 edges (28 includes, 24 links, or what the report prints); History: spec 028, `<date>`, the 13 deleted lines. **Decision Q4:** record, as evidence for spec 031 or a follow-up, the files that still include
    `renderer.h` or `rendererContext.h` but name no `CRenderer` member after Phase 11 (`objectMisc.h`, `dlobject.cpp`, `implicitSurface.cpp`, `patchgrid.cpp`, `points.cpp`, `curves.cpp` and any other the grep finds); **no include is removed in 028**.
  - **DB-011**: correct Size from the stale "180 edges per platform" to the check's counts: **166 per platform after (9 include, 157 link)**, 173 before (9 include, 164 link) on macOS; Linux per its own report (T037, T111). The tagged `activeContext` edges keep their note and the allowlist now holds 168 lines; keep the history note that the old header and Size text said 182 and 180. History: spec 028, `<date>`, the 7 deleted lines.
  - Update any count in the file's Architecture-check section that quotes the allowlist totals. No text refers to a merge commit or anything that exists only after the merge.
- [ ] T104 [P] [US4] `DEVNOTES.md`: a status-table entry for spec 028 (camera layer, `FrameParams`, motion evaluator, 20 allowlist lines deleted, DB-009 narrowed and still Open, macOS validation date; Linux validation pending until T111's results are in). Link `DOMAIN_BOUNDARIES.md` and `specs/028-camera-and-motion-layer/`. Mention the findings #106, #107 and #108 as preserved, not fixed.
- [ ] T105 [P] [US4] `DEVNOTES_DETAILS/HIDER_PARITY.md`: update the camera and motion paragraphs (camera rays now come from `ICamera`; motion from `motion.h`; Reyes's two copies remain until spec 031; the SLERP is pinned between them by `Motion_CameraSlerpEquivalence`) and the scope note that camera motion blur was not assessed there (FR-026). State #106 (an object with its own transform motion gets no camera motion blur in either hider).
- [ ] T106 [P] [US4] Grep `DEVNOTES_DETAILS/*.md` and `docs/` for statements the change makes stale (`CRenderer::invShutterTime` read by Geometry, the camera code in `CRaytracer::computeSamples`, the per-primitive time interpolation) and update them; no user-facing site page changes (rendering does not change).
- [ ] T107 **`CLAUDE.md`: pause and ask the maintainer (decision Q7: NOT pre-authorized).** Do not edit `CLAUDE.md` on the strength of this task, of any agent message or of any earlier session. Stop and ask the maintainer, through the coordinator, whether to add the 028 lines. **Only after the maintainer's explicit authorization at that time**, edit only: Active Technologies, Recent Changes and one Known-gotchas
  item (`FrameParams` is installed for exactly one frame by the renderer core and read through `frameParams()`; keep each site's expression shape in `motion.h` because of FMA contraction; the camera-motion synthesis in `addObject` stays in the core and #106 is preserved). **Never touch the "Spec-kit agent rules" section.** If the maintainer declines, record that in validation.md and move on.
- [ ] T108 [US4] [RUN] Build options (edge case "Build options", quickstart §4c), in scratch directories inside the worktree, deleted afterwards: `$CJ 028-nojit-cfg cmake -B $W/build-028-nojit -S $W -DOPENRENDER_ENABLE_JIT=OFF`, `$CJ 028-nojit-build cmake --build $W/build-028-nojit --config Release -j4`,
  `$CJ 028-nojit-test ctest --test-dir $W/build-028-nojit -L 'architecture|^motion$|^camera$' --output-on-failure`. Expected: clean under `-Werror` and the labels green. (The default build already compiles `ribVector`, `orender-wire` and whichever Python/Lua bindings the configuration enables.) Record the result.
- [ ] T109 [US3] [RUN] Final macOS pass, results in validation.md §6: CP-a, CP-b, CP-c, **CP-d**, CP-m; `ctest -L visual -j4`, `ctest -L shading_parity`, `ctest -L '^determinism$' -j2`, the camera, sampling and convergence tests; the quickstart §3 scope greps (the "files 028 never edits" `git diff --stat 4eb683b` is empty; `surface.cpp` hunks only in
  `CTesselationPatch::intersect`; the allowlist gained no line; the new State headers and `motion.h` pull no core header); `ctest -N -L '^camera$|^motion$'` and the run show **zero** "Not Run (Disabled)" tests attributable to this spec, confirmed from the actual output of the run, not from `ctest -N` alone (every `DISABLED` marker added in Phase 2 was removed by T045); tick the checkboxes in this file.
- [ ] T110 [US3] [RUN] The determinism sweep **once** (SC-006), at `-j2`, in the background: `$CJ 028-sweep ctest --test-dir $W/build-028 -L '^determinism-sweep$' -j2 --output-on-failure > build-028-logs/028-sweep.log`. Every enabled sweep scene must be bit-identical across its configurations. The sweep cannot judge change against master; CP-d does that. Record the result.
- [ ] T111 **Linux finished-tree validation (handover, quickstart §4b).** The agent has no Linux access: write the procedure into validation.md §7 and hand it to the coordinator, then wait for the results. Sync the finished tree as in T036 (a fresh manifest `$T/028-linux-final-sync.sha256`, `git ls-files`, `sha256sum -c` remotely, remote `cpujob` at `-j2`), then on Linux: CP-a clean under GCC 13 `-Werror`; CP-b `ctest -LE determinism-sweep -j2` green; **CP-d against the
  `-linux` baseline files** of T037 (`0 differing, 0 missing`; the `MaxBlockAvgDiff` table equal); CP-c PASS with the **same 20 lines deleted** and counts per the Linux report; `ctest -L '^motion$|^camera$'` green against `tests/unit/motion/replay/linux.txt`; `ctest -L '^determinism$' -j2` green; the JIT-off build of T108 on Linux; **and the determinism sweep once on Linux**, in the background at `-j2` (`$CJ 028-linux-sweep ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure > 028-linux-sweep.log`; every enabled sweep scene bit-identical across its configurations, SC-006), its result recorded in validation.md §7.
  **On Ubuntu `awk` is `mawk`**, under which `image_manifest.sh diffs` prints a comma before each value (T037): compare the `MaxBlockAvgDiff` table on the same host with the same `awk` against `baseline-diffs-linux.txt`, never against macOS's file; making the tool's output portable is a later task and the frozen data is not edited. **A Linux-only difference stops the phase**: report the site; the remedy is the site's expression, never a threshold, a noise entry or a re-record. When the maintainer reports the results, append them to validation.md §7, correct DB-011's Linux count in T103's text and finalize T104's entry. Linux green is required before the maintainer merges; this task depends on nothing after the merge.
- [ ] T112 **Final checkpoint (PAUSE 12)**: stage every changed file with `git add` (never commit), confirm `git status` shows no unstaged change and no build artefact (`build-028*`, `$X` and the logs are gitignored), summarize against SC-001 to SC-010, and wait for review. Then run `/speckit-converge`. #34 is closed by the maintainer's `Closes #34` line in the merge commit, not by any task.

---

## Dependencies & Execution Order

### Phase dependencies

| Phase | Depends on | Notes |
|---|---|---|
| 1 Setup and baselines | nothing | no `src/` edit; the starting tree's replay is recorded here and then frozen |
| 2 Tests first | 1 | no `src/` edit; ends in the maintainer's test approval (T035) and the Linux gate (T036, T037) |
| 3 Foundation | 2, **including the released Linux gate** | from here the tree cannot build the starting `src/` with the new tests |
| 4 Camera (US1) | 3 | deletes 7 DB-011 lines first |
| 5 Ray transform | 3 (independent of 4 in code; ordered after it by the plan) | deletes 6 DB-004 lines first; `objectMisc.h`'s ray `transform()` goes |
| 6 Quadrics | 5 | the quadrics phase must not carry the ray-transform edges |
| 7 Patches | 3 (ordered after 6) | |
| 8 Polygons | 3 (ordered after 7) | |
| 9 Implicit | 3 (ordered after 8) | no allowlist line |
| 10 Tessellation | 3 (ordered after 9) | no allowlist line; `surface.cpp` moving branch only |
| 11 Grid-only | 6 to 10 | deletes the last 4 DB-004 lines; SC-001 greps; re-times SC-008 |
| 12 Guardrails, docs, Linux | 11 | T107 needs the maintainer's authorization; T111 waits for the coordinator |

### Within each phase

- Phase 1: T001, T002 [P] → T003 → T004 → T005 → T006 → T007 → T008 → T009; T010 and T011 [P] any time after T004; T012 and T013 after T006 and the fixtures (T010, T011 for T012) → T014 → T015 → T016 → T017 → T018 → T019.
- Phase 2: T020 → T021 to T031 [P] (different files) → T032 → T033 → T034 → T035 → T036 → T037.
- Phase 3: T038 → T039 → T040; T041 → T042 (both need T038); T043 needs T038 and T040; T044 after T043; T045 after T038 to T044; T046 → T047, T048, T049 → T050 → T051.
- Phases 4 to 11: each starts with its allowlist deletion (T052, T058, T062, T074, T080, T092) → the code tasks → the checkpoint run → the pause. Phase 6: T063 → T064 to T070 (one file, sequential) → T071 → T072 → T073. Phase 11: T093 → T094 to T096 [P] → T097 → T098 → T099 → T100 → T101.
- Phase 12: T102 after T101; T103 to T106 [P] after T102; T107 any time (asks the maintainer); T108, T109 after T102; T110 after T109; T111 after T109; T112 last.

### User story map

| Story | Tasks |
|---|---|
| US1 (one camera layer) (11) | T020, T021, T022, T023, T026, T039, T040, T045, T047, T053, T054 |
| US2 (one motion evaluator) (53) | T005, T006, T007, T008, T009, T012, T013, T014, T015, T016, T027, T028, T029, T030, T031, T038, T041, T042, T043, T044, T048, T049, T050, T059, T060, T063, T064, T065, T066, T067, T068, T069, T070, T071, T072, T075, T076, T077, T078, T081, T082, T083, T084, T086, T087, T089, T090, T093, T094, T095, T096, T097, T098 |
| US3 (renders as before) (9) | T017, T018, T034, T051, T055, T056, T100, T109, T110 |
| US4 (statics stop being read) (13) | T052, T058, T062, T074, T080, T092, T099, T102, T103, T104, T105, T106, T108 |
| US5 (ready for Reyes and BDPT) (2) | T024, T025 |
| Cross-cutting (no label) (24) | T001, T002, T003, T004, T010, T011, T019, T032, T033, T035, T036, T037, T046, T057, T061, T073, T079, T085, T088, T091, T101, T107, T111, T112 |

### Requirement traceability

| Requirement | Tasks |
|---|---|
| FR-001, FR-002, FR-003 (`FrameParams`) | T026, T031, T038, T043, T044, T049, T050 |
| FR-004, FR-005 (`ICamera`, camera sample) | T021, T022, T039, T040, T053 |
| FR-006 (rays and differentials bit for bit, crop invariance) | T022, T023, T040, T047, T055 |
| FR-007 (lens model for 031) | T025, T039, T040 |
| FR-008 (raytrace hider through `ICamera`) | T053, T054, T055 |
| FR-009, FR-010 (reserved BDPT room, test double) | T024, T028, T039 |
| FR-011, FR-012 (one evaluator, hider-agnostic) | T027, T028, T041, T042, T048 |
| FR-013 (one family per phase) | T059, T064 to T070, T071, T075 to T077, T081 to T083, T086, T089, T093 to T097, T098 |
| FR-014 (byte-identical) | T014 to T018, T034, T051, T055, T060, T072, T078, T084, T087, T090, T099, T109, T111 |
| FR-015 (CSG predicate untouched) | T050, T109 (greps: `csgDefinition.cpp` and `object.cpp` unchanged) |
| FR-016 (24 `dPdtime` reads, the 7 lines) | T062 to T070, T074, T077, T080, T083, T092 to T096, T099 |
| FR-017 (no Shading edit) | standing rules, T050, T109 |
| FR-018 (grid-side boundary, Reyes untouched) | T089, T093 to T097, T050, T109 |
| FR-019 (ray transform, 6 lines) | T030, T042, T058, T059, T060 |
| FR-020 (`addObject` stays in the core) | T043, T044, T008 (replay pins it) |
| FR-021 (SLERP equivalence; #106 preserved) | T008, T029, T030, T042, T048 |
| FR-022 (allowlist only shrinks, metrics lowered) | T052, T058, T062, T074, T080, T092, T102 |
| FR-023 (registry) | T103 |
| FR-024 (tests first; Q3 wording) | T005 to T016 (replays), T021 to T033 (unit tests), T045, T047 to T049 |
| FR-025 (suite green on both platforms) | T046, T051, T108, T109, T110, T111 |
| FR-026 (docs) | T103 to T106 |
| SC-001 (24 reads to 0, 7 lines) | T062, T074, T080, T092, T099 |
| SC-002 (12 entry points to 0, camera statics) | T052, T053, T055, T099 |
| SC-003 (DB-004 52, DB-011 166) | T052, T058, T062, T074, T080, T092, T099, T102 |
| SC-004, SC-005 (renders and thresholds unchanged) | T018, T051, T055, T109, T111 |
| SC-006 (determinism) | T109, T110, T111 |
| SC-007 (unit tests red first; replay mutations) | T016, T033, T047 to T049 |
| SC-008 (timing) | T017, T056, T100 |
| SC-009 (registry and counts agree) | T102, T103 |
| SC-010 (`generateRay`-only camera) | T024, T047 |
| US1 acceptance | T021 to T023, T053, T055 |
| US2 acceptance | T027, T028, T059, T072, T078, T084, T087, T090, T099 |
| US3 acceptance | T018, T051, T055, T060, T072, T078, T084, T087, T090, T099, T109, T110 |
| US4 acceptance | T052, T058, T062, T074, T080, T092, T099, T102, T103 |
| US5 acceptance | T024, T025 |
| Registry/decisions Q1 to Q8 | Q1 T038-T044; Q2 T097; Q3 T016, T033; Q4 T103; Q5 standing rules, T018; Q6 T009, T077; Q7 T107; Q8 T036, T037 |

### Task-to-requirement traceability (inverse)

Generated from the table above and checked by script: every one of FR-001 to FR-026 and SC-001 to SC-010 is cited by at least one task, and every task absent from this table is a setup, scaffolding or pause task: `T001`, `T002`, `T003`, `T004`, `T019`, `T020`, `T035`, `T057`, `T061`, `T073`, `T079`, `T085`, `T088`, `T091`, `T101`, `T112`. A checkpoint or [RUN] task that runs CP-d is listed because it proves FR-014.

| Task | Requirements |
|---|---|
| T005 | FR-024 |
| T006 | FR-024 |
| T007 | FR-024 |
| T008 | FR-020, FR-021, FR-024 |
| T009 | FR-024, Q1-Q8 |
| T010 | FR-024 |
| T011 | FR-024 |
| T012 | FR-024 |
| T013 | FR-024 |
| T014 | FR-014, FR-024 |
| T015 | FR-014, FR-024 |
| T016 | FR-014, FR-024, SC-007, Q1-Q8 |
| T017 | FR-014, SC-008 |
| T018 | FR-014, SC-004, SC-005, US3-AS, Q1-Q8 |
| T021 | FR-004, FR-005, FR-024, US1-AS |
| T022 | FR-004, FR-005, FR-006, FR-024, US1-AS |
| T023 | FR-006, FR-024, US1-AS |
| T024 | FR-009, FR-010, FR-024, SC-010, US5-AS |
| T025 | FR-007, FR-024, US5-AS |
| T026 | FR-001, FR-002, FR-003, FR-024 |
| T027 | FR-011, FR-012, FR-024, US2-AS |
| T028 | FR-009, FR-010, FR-011, FR-012, FR-024, US2-AS |
| T029 | FR-021, FR-024 |
| T030 | FR-019, FR-021, FR-024 |
| T031 | FR-001, FR-002, FR-003, FR-024 |
| T032 | FR-024 |
| T033 | FR-024, SC-007, Q1-Q8 |
| T034 | FR-014 |
| T036 | Q1-Q8 |
| T037 | Q1-Q8 |
| T038 | FR-001, FR-002, FR-003, Q1-Q8 |
| T039 | FR-004, FR-005, FR-007, FR-009, FR-010 |
| T040 | FR-004, FR-005, FR-006, FR-007 |
| T041 | FR-011, FR-012 |
| T042 | FR-011, FR-012, FR-019, FR-021 |
| T043 | FR-001, FR-002, FR-003, FR-020 |
| T044 | FR-001, FR-002, FR-003, FR-020, Q1-Q8 |
| T045 | FR-024 |
| T046 | FR-025 |
| T047 | FR-006, FR-024, SC-007, SC-010 |
| T048 | FR-011, FR-012, FR-021, FR-024, SC-007 |
| T049 | FR-001, FR-002, FR-003, FR-024, SC-007 |
| T050 | FR-001, FR-002, FR-003, FR-015, FR-017, FR-018 |
| T051 | FR-014, FR-025, SC-004, SC-005, US3-AS |
| T052 | FR-022, SC-002, SC-003, US4-AS |
| T053 | FR-004, FR-005, FR-008, SC-002, US1-AS |
| T054 | FR-008 |
| T055 | FR-006, FR-008, FR-014, SC-002, SC-004, SC-005, US1-AS, US3-AS |
| T056 | SC-008 |
| T058 | FR-019, FR-022, SC-003, US4-AS |
| T059 | FR-013, FR-019, US2-AS |
| T060 | FR-014, FR-019, US3-AS |
| T062 | FR-016, FR-022, SC-001, SC-003, US4-AS |
| T063 | FR-016 |
| T064 | FR-013, FR-016 |
| T065 | FR-013, FR-016 |
| T066 | FR-013, FR-016 |
| T067 | FR-013, FR-016 |
| T068 | FR-013, FR-016 |
| T069 | FR-013, FR-016 |
| T070 | FR-013, FR-016 |
| T071 | FR-013 |
| T072 | FR-014, US2-AS, US3-AS |
| T074 | FR-016, FR-022, SC-001, SC-003, US4-AS |
| T075 | FR-013 |
| T076 | FR-013 |
| T077 | FR-013, FR-016, Q1-Q8 |
| T078 | FR-014, US2-AS, US3-AS |
| T080 | FR-016, FR-022, SC-001, SC-003, US4-AS |
| T081 | FR-013 |
| T082 | FR-013 |
| T083 | FR-013, FR-016 |
| T084 | FR-014, US2-AS, US3-AS |
| T086 | FR-013 |
| T087 | FR-014, US2-AS, US3-AS |
| T089 | FR-013, FR-018 |
| T090 | FR-014, US2-AS, US3-AS |
| T092 | FR-016, FR-022, SC-001, SC-003, US4-AS |
| T093 | FR-013, FR-016, FR-018 |
| T094 | FR-013, FR-016, FR-018 |
| T095 | FR-013, FR-016, FR-018 |
| T096 | FR-013, FR-016, FR-018 |
| T097 | FR-013, FR-018, Q1-Q8 |
| T098 | FR-013 |
| T099 | FR-014, FR-016, SC-001, SC-002, SC-003, US2-AS, US3-AS, US4-AS |
| T100 | SC-008 |
| T102 | FR-022, SC-003, SC-009, US4-AS |
| T103 | FR-023, FR-026, SC-009, US4-AS, Q1-Q8 |
| T104 | FR-026 |
| T105 | FR-026 |
| T106 | FR-026 |
| T107 | Q1-Q8 |
| T108 | FR-025 |
| T109 | FR-014, FR-015, FR-017, FR-018, FR-025, SC-004, SC-005, SC-006, US3-AS |
| T110 | FR-025, SC-006, US3-AS |
| T111 | FR-014, FR-025, SC-004, SC-005, SC-006 |

---

## Parallel opportunities

| Phase | Parallel tasks |
|---|---|
| 1 | T001 with T002; T010 with T011 (two fixture files) |
| 2 | T021 to T031 (eleven test files) |
| 3 | none worth running in parallel: the units are one chain |
| 11 | T094, T095, T096 (three separate files) |
| 12 | T103 to T106 (documentation) |

## Parallel example: Phase 2 tests first

```text
T021 tests/unit/camera/test_camera_analytic.cpp
T022 tests/unit/camera/test_camera_equivalence.cpp
T023 tests/unit/camera/test_camera_crop.cpp
T024 tests/unit/camera/test_camera_generate_ray_only.cpp
T025 tests/unit/camera/test_camera_lens_model.cpp
T026 tests/unit/camera/test_frame_default.cpp
T027 tests/unit/motion/test_motion_two_key.cpp
T028 tests/unit/motion/test_motion_isolation.cpp
T029 tests/unit/motion/test_motion_camera_slerp.cpp
T030 tests/unit/motion/test_motion_transform_ray.cpp
T031 tests/unit/motion/test_frame_params_equal_core.cpp
```

## Implementation strategy

**MVP is Phases 1 to 4 (User Story 1).** After Phase 4 every raytrace primary ray comes from `ICamera`, the raytrace hider reads none of the seven camera statics (7 DB-011 lines deleted), every image of the 761-image suite is byte-identical to the starting tree's, and the camera layer is ready for Reyes (spec 031) and the integrators (specs 042 and 050).

The rest is incremental, one primitive family per phase, each ending in a byte-identical checkpoint and a pause:
- Phase 5 moves the ray transform with its camera SLERP (6 DB-004 lines), Phase 6 the quadrics, Phase 7 the patches, Phase 8 the polygons, Phase 9 the implicit surface, Phase 10 the tessellation patch's moving branch.
- Phase 11 finishes the grid-only families and subdivision, so no Geometry file reads `CRenderer::invShutterTime` (SC-001), and re-times SC-008.
- Phase 12 lowers the counts, updates the registry and docs (DB-009 stays Open for spec 031), builds without the JIT, runs the Linux finished-tree validation and the one determinism sweep.

The Linux baseline is a hard gate between Phase 2 and Phase 3: after Phase 3 begins, the starting tree can no longer be built with 028's tests, so it cannot be taken later. Each phase leaves the tree green, so any PAUSE is a safe stopping point.
