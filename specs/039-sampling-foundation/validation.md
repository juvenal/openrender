# Validation: Sampling Foundation (spec 039)

Results are appended per phase. Phase 0 is the baseline confirmation.

## Phase 0: baseline confirmation (T001-T004)

Run 2026-10-16 on the maintainer's fanless M2 (macOS, arm64), one job at a time, `nice -n 19`, `-j4`.

### T001: pristine baseline

- `git rev-parse HEAD`: `3560485c212c5e36ef8e18f1aaabc60a04e0d47c` (branch `039-sampling-foundation`). Working tree clean before and after.
- `git diff --stat c62334d -- src tests CMake CMakeLists.txt shaders examples`: **printed nothing** (exit 0). Only `specs/` differs from master `c62334d`.
- Configure: `cmake -B build -S .` (6 s). `build/` was absent.
  - Dependencies: **vcpkg** (`VCPKG_ROOT=/Users/juvenal/.vcpkg`, `VCPKG_INSTALLED_DIR=/Users/juvenal/.cache/vcpkg-installed/openrender` were set in the shell). Toolchain `~/.vcpkg/scripts/buildsystems/vcpkg.cmake`, triplet `arm64-osx-openrender`, LLVM 23.1.1 core-only, libpng 1.6.58, tiff 4.7.2. Not Homebrew.
  - Generator: Unix Makefiles. JIT: `OPENRENDER_ENABLE_JIT=ON`.
- Build type: **Release**. `CMAKE_BUILD_TYPE` is empty in the cache; the root `CMakeLists.txt` (line 189) defaults it to Release. The main checkout's cache says `Release` explicitly, so the two builds use the same flags. Built with `nice -n 19 cmake --build build --config Release -j4`: exit 0, **84 s** wall clock, no compiler warnings (only linker `ld: warning` lines about duplicate libraries and the Swift `orender-wire-macos` 13.3-vs-13.0 deployment-target mismatch).
- `ctest -N` lists **598** tests here, and also **598** in the main checkout's `build/` (same set; the name lists are identical). The 599 quoted for `ad0ee50` does not match either tree at `c62334d`; it is not a difference introduced by this worktree.

### T002: baselines

#### M1: `-P:N` bit-identity on today's code (research §M1; identical in every configuration)

Master `c62334d`, `jitter 0` float TIFFs compared with `tiffcmp` against `t1a` (`-t:1`); negative control (opaque vs transparent) differed, so the comparison is live. All entries IDENTICAL:

| Scene | t1b | p2t1 (`-P:2 -t:1`) | p2 (`-P:2`) | p4t1 (`-P:4 -t:1`) | tN (`-t:4`) |
|---|---|---|---|---|---|
| color-determinism-opaque-reyes | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL |
| color-determinism-transparent-reyes | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL |
| depthfilter-determinism-avg-reyes | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL |
| depthfilter-determinism-mid-reyes | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL |
| depth-analytic-min-raytrace | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL |
| coverage-alpha-sides2-raytrace | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL |
| wide-filter-reyes (gaussian 3x3, 16x16 buckets) | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL |

- Outcome: `-P:N` stays in SC-001 and FR-030 as written; no `fix/` prerequisite.
- **Caveat (a)**: `orender` disables threads whenever `-P` is combined with an explicit `-t`, so `p2t1` and `p4t1` ran single-threaded children. The determinism harness's `-P` entry is therefore written `-P:2`, never `-P:2 -t:1`.
- Caveat (c): only `jitter 0` scenes were measured, by design.

#### M2: performance baseline (research §M2; SC-010, FR-039)

Raw times: `specs/039-sampling-foundation/tools/perf/m2-baseline-c62334d.txt` (columns: scene, cfg, rep, seconds; 5 interleaved repetitions, rep-major). Master `c62334d`, Release, seconds. Spread is (max-min)/median; MAD is the median absolute deviation over the median.

| Scene | t1 median | min | max | spread % | MAD % | tdef median | min | max | spread % | MAD % |
|---|---|---|---|---|---|---|---|---|---|---|
| camera-dof-reyes | 3.914 | 3.908 | 4.031 | 3.1 | 0.1 | 2.019 | 2.013 | 2.061 | 2.4 | 0.3 |
| camera-dof-raytrace | 2.548 | 2.540 | 2.556 | 0.6 | 0.2 | 1.324 | 1.320 | 1.341 | 1.6 | 0.2 |
| motion-3-reyes | 4.485 | 4.475 | 4.494 | 0.4 | 0.0 | 2.307 | 2.304 | 2.318 | 0.6 | 0.1 |
| quadlight | 2.329 | 2.309 | 2.359 | 2.2 | 0.5 | 1.195 | 1.194 | 1.199 | 0.4 | 0.0 |
| sphere-gather-reyes | 2.837 | 2.827 | 2.851 | 0.9 | 0.2 | 1.547 | 1.542 | 1.571 | 1.9 | 0.3 |
| sphere-trace-reyes | 2.170 | 2.164 | 2.212 | 2.2 | 0.3 | 2.168 | 2.160 | 2.208 | 2.2 | 0.4 |

- **SC-010 compares medians of 5 or more interleaved repetitions, never single runs.** Single runs reach about 3% above the median (camera-dof-reyes `t1` max), MAD is 0.5% or less, so a median of 5 resolves a 2% change. Pass rule: every scene's median, in both `t1` and `tdef`, at most 2% above the baseline median, measured on the same machine and Release configuration with the default sampler.
- Note: `-P` with `-t` disables threads in the children (research §M1, caveat a).
- `sphere-trace-reyes` pins `numthreads 1`, so its `tdef` equals its `t1`; the pin stays in the perf copy.
- This worktree's build was not used to re-measure M2; the numbers are the coordinator's, taken from the main checkout's build of the same code.

### T003: full suite on the baseline

`nice -n 19 ctest --test-dir build -j4 --output-on-failure`: **597 passed, 1 failed, out of 598** (99%), 115 s real time. Reference for "unchanged under `Legacy`" in Phases 1-9.

Failure: `#175 Parity_transparency-matte-aov` (known flaky, in the maintainer's list):

```
FAIL: 1 block(s) exceed threshold 25.00. Worst avg diff=27.73 on channel R at block (28,13)
Blocks: 1200 (8x8px each)  MaxBlockAvgDiff: 27.73 (threshold: 25.00)  FailBlocks: 0 -> FailBlocks: 1
```

Re-run in isolation (`ctest -R '^Parity_transparency-matte-aov$'`), 10 runs: **10/10 passed**. The four runs with `-V` printed MaxBlockAvgDiff **23.83, 23.91, 23.88, 23.88** (threshold 25.00). The threshold was not touched. The in-suite value (27.73) was taken with `-j4` contention; it is the same scene that is on the flaky list.

Other watched tests:
- `Visual_sphere-shader-parameter-reyes`: passed in the full run, and 3/3 in isolation. `-V` MaxBlockAvgDiff: **5.91** (reyes), **6.00** (reyes-slo), threshold 20.
- `ImagerRegressionRender`, `Parity_depthdefault`, `ShadingParity_AmbientAccumulation`: all passed in the full run.
- Architecture label: 21 tests, all passed (included in the full run).

Observation for T005: the label `sampling` already exists on **`DiskSampling`** (test #2, 1 test). T005 registers `test_sampling` under label `sampling`, so `-L sampling` will then select `DiskSampling` too. Harmless to the plan, but worth knowing when a phase says "run `-L sampling`".

### T004: shader staleness

- `build/src/oshader/oshader` mtime: 2026-10-16 08:43:35.
- `build/shaders/`: **105 `.rslo` and 105 `.slo`** (matches the 105 `.sl` sources). `find build/shaders \( -name '*.rslo' -o -name '*.slo' \) ! -newer build/src/oshader/oshader` printed nothing: **no compiled shader is older than `oshader`**. Not stale.
- `build/` is a fresh build from this tree, so this is the expected result. The deploy-tree `openrender/` was not used by any test.

## Phase 1: setup (T005-T008)

Run 2026-10-16 on top of the committed Phase 0 (`2f921b2`), same `build/` (vcpkg, Release), `nice -n 19`, `-j4`, one job at a time.

### T005-T007: what was added

- `tests/unit/sampling/CMakeLists.txt` and `test_sampling.cpp` (a `return 0` placeholder): `test_sampling`, C++20, links the **shared** `openrendercommon` (`otool -L` shows `@rpath/libRIBCommon.1.dylib`). **No test is registered.** Registering a vacuous `main` would add a green test that proves nothing, so the target exists but `add_test` waits for Phase 2.
- `tests/convergence/CMakeLists.txt`, `test_convergence.cpp` (placeholder), `README.md` and `reference/.gitkeep`: `test_convergence`, links `TIFF::TIFF`. No test is registered, so nothing carries the `convergence` label yet and the default run cannot include it; `ctest -N -L convergence` reports 0 tests. The opt-in rule is recorded in the CMake comment for Phase 8.
- `tests/CMakeLists.txt`: `add_subdirectory(unit/sampling)` and `add_subdirectory(convergence)`.
- `tests/visual/CMakeLists.txt`: a comment reserving the `determinism-sweep` label, next to `add_depth_determinism_test`. No test is registered.

### How `ctest -L sampling` behaves after T005

The `sampling` label is **not empty** and stays as it was: it selects the one test `DiskSampling` (`tests/CMakeLists.txt`, labels `sampling;core`), which predates spec 039. `ctest -L sampling` therefore reports 1 test, passing. T005 added no test, so the count did not change (still **598** tests in total).

From Phase 2 on, the new `Sampling_*` tests are registered with `LABELS "sampling;unit"`, so `-L sampling` selects them **and** `DiskSampling`. `DiskSampling` was not renamed or relabelled (tasks.md does not ask for it). Anything that needs only the new tests should select them by name: `-R '^Sampling_'`.

The task text says "an empty `sampling` set"; that expectation is not literally true because of `DiskSampling`. "Green, with no new tests" is what was observed.

### T008: build and run

- `cmake -B build -S .` and `cmake --build build --config Release -j4`: both exit 0; 6 s together (incremental, only the two new executables compiled and linked), no compiler warnings.
- `ctest -L sampling`: **1/1 passed** (`DiskSampling`).
- `ctest -L "sampling|architecture"`: **22/22 passed** (21 architecture tests plus `DiskSampling`).
- `ctest -N -L convergence`: 0 tests. `ctest -N` total: 598 (unchanged).
- archcheck does **not** classify `tests/` paths: `domain-map.txt` has no `tests/` prefix (only `src/libshader/tests/`), and the archcheck run over the finished build reports `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0` with no mention of `test_sampling`, `test_convergence`, `unit/sampling` or `tests/convergence`.
- archcheck also prints metric warnings (`Geometry refs 78 > baseline 76`, `Hiders refs 586 > 581`, `TOTAL refs 749 > 742`). They are **not caused by Phase 1**: the main checkout's `build/` at `c62334d` prints the identical lines and totals. They are warnings, not failures.

## Phase 2: sampler library, tests first (T009-T017)

Run 2026-10-16 on top of the committed Phase 1 (`eb39f76`), `nice -n 19`, `-j4`, one job at a time. No implementation file was written: `src/common/sampling.h` does not exist.

### Tests written (14 ctest tests in 9 executables, `tests/unit/sampling/`)

| Task | Source | Executable | ctest tests (case argument) |
|---|---|---|---|
| T009 | `test_reference.cpp` | `test_sampling_reference` | `Sampling_SobolReference` (`sobol_reference`) |
| T010 | `test_range_purity.cpp` | `test_sampling_range_purity` | `Sampling_Range` (`range`), `Sampling_Purity` (`purity`) |
| T011 | `test_stratification.cpp` | `test_sampling_stratification` | `Sampling_NetProperty` (`net`), `Sampling_PMJ02Property` (`pmj02`) |
| T012 | `test_uniformity.cpp` | `test_sampling_uniformity` | `Sampling_Uniformity` (`uniformity`), `Sampling_Discrepancy` (`discrepancy`) |
| T013 | `test_independence.cpp` | `test_sampling_independence` | `Sampling_DrawIndependence`, `Sampling_Padding`, `Sampling_Composite3D4D` |
| T014 | `test_convergence_smooth.cpp` | `test_sampling_convergence_smooth` | `Sampling_ConvergenceSmooth` |
| T015 | `test_camera_grid.cpp` | `test_sampling_camera_grid` | `Sampling_CameraGridConsistency` |
| T016 | `test_keys.cpp` | `test_sampling_keys` | `Sampling_Keys` (`keys`) |
| T016 (folded in) | `test_keyed_stream.cpp` | `test_sampling_keyed_stream` | `Sampling_KeyedStream` (`keyed_stream`) |

- Shared helpers: `sampling_test_util.h` (check macro, `splitmix`, a chi-square p-value, case dispatch).
- All eight targets are `EXCLUDE_FROM_ALL`; the 13 tests are registered with `LABELS "sampling;unit"`. They need no fixture or reference file (every expected value is computed inside the test), so there is nothing else to register.
- The Phase 1 placeholder `test_sampling` is untouched.
- The tests use only the contract's API plus `floorDiv(int32_t, int32_t)` and `floorMod(int32_t, int32_t)` (named in T022, not in the contract header) in namespace `openrender::sampling`, and include `"common/sampling.h"`.

### T017: red evidence

- Default build (`cmake --build build --config Release -j4`): exit 0, so the default build is still green.
- `ctest -N`: **611** tests (598 + 13 new).
- Each target built on its own (`cmake --build build --target test_sampling_<x>`): **8 of 8 fail to compile**, exit 2, each with the single error
  `tests/unit/sampling/sampling_test_util.h:18:10: fatal error: 'common/sampling.h' file not found`.
- `ctest -L sampling`: **14 tests, 1 passed (`DiskSampling`), 13 "Not Run"** (executables absent). `Sampling_SobolReference`, `_Range`, `_Purity`, `_NetProperty`, `_PMJ02Property`, `_Uniformity`, `_Discrepancy`, `_DrawIndependence`, `_Padding`, `_Composite3D4D`, `_ConvergenceSmooth`, `_CameraGridConsistency`, `_Keys` are all in the failed list.
- `ctest -L architecture`: 21/21 passed.
- Because the 13 tests are registered while their executables cannot be built, **a full `ctest` run is red (13 "Not Run") until T024**. This is by design.

### Checks made without an implementation

- All eight sources pass `clang++ -std=c++20 -Wall -Wextra -Werror -fsyntax-only` against a declaration-only stub of `contracts/sampler-api.md` kept outside the repository, so the first compile error after T018 should be an implementation mismatch, not a typo.
- The test-side helpers were run on their own: the closed-form Sobol dimension 1 reproduces the hand-written values 0, 1/2, 3/4, 1/4, 5/8, 1/8, 3/8, 7/8; van der Corput gives 1/2, 1/4, 3/4, 1/8; the net checker reports 0 violations for the first 4096 points of (van der Corput, Sobol dim 1) and violations for a diagonal; the star-discrepancy routine gives 0.75 for the single point (0.5, 0.5), and about 0.164, 0.082, 0.036 for 8-key random means at N = 64, 256, 1024; the chi-square p-value gives 0.01 at 92.01 with 63 dof.

### Choices the tests make that T018-T022 must honour (for approval)

1. **Sobol dimension 1 is in natural (index) order**, i.e. the Pascal generator matrix applied to the index bits, not in Gray-code order.
2. `Sampling_ConvergenceSmooth` requires **PMJ02 as well as Sobol** to reach a slope of -0.9 or steeper, and Independent -0.4 or steeper. The task text names only "the default backend".
3. `Sampling_Discrepancy` compares the **mean over 8 keys** of the exact 2D star discrepancy at each N, not each key against each key. The measured values are printed.
4. `Sampling_Uniformity` uses one fixed key and 16 dimensions per backend. The pass is deterministic, but Independent has about a 15% chance of failing one of 16 tests at p = 0.01 by luck of its hash, as the test is specified.
5. `Sampling_Purity` runs the 8-thread pass **before** the serial pass, with a start barrier, so that lazily built PMJ02 tables are first touched concurrently.
6. `Sampling_Keys` does not assert `SampleKey::kind` (no enum names are fixed in the contract). It asserts that a child of a cell key keeps `pixelSampleIndex == -1` and `path == full`.
7. Not covered by any listed test, and so not written at first: the camera-slot accessors (`cameraFilm`, `cameraLens`, `cameraTime`, `cameraImportance`). `CKeyedStream` was added afterwards as `Sampling_KeyedStream` (see the amendments below).

### Amendments after the test review (maintainer's two changes)

Made without any build or ctest run (disk space); checked with `-fsyntax-only` against the scratch stub, and a `cmake -B build -S .` configure to confirm the registration.

1. **`Sampling_Uniformity` (item 4 of the list above is superseded).** The 48 chi-square checks (3 backends x 16 dimensions) are one family, Bonferroni-corrected: each must reach p >= 0.01 / 48 (about 2.1e-4) instead of p >= 0.01. A correct generator now fails the whole test about 1% of the time instead of about 38% (1 - 0.99^48), while a biased one (p near 0) still fails every dimension it affects. The reason is a comment in `test_uniformity.cpp`; the rule is the same for all backends. The printed per-backend minimum p now also shows the per-check level. The wording of T012 in `tasks.md` was updated to match.
2. **New `Sampling_KeyedStream`** (`test_keyed_stream.cpp`, executable `test_sampling_keyed_stream`, `LABELS "sampling;unit"`, `EXCLUDE_FROM_ALL`), folded into T016's line in `tasks.md` (no renumbering). It covers `CKeyedStream`: the same key gives an identical sequence (mixed `next1D`/`nextU32` calls, and a short run is a prefix of a long one); `next1D` lies in [0, 1 - 2^-24] with mean within 5 sigma of 0.5; `nextU32` has every one of its 32 bits set about half the time (5 sigma, 2^16 draws); different keys (adjacent, and differing in one high bit) give no equal draw at the same position over 2^14 draws and a correlation below 5/sqrt(N); a stream shares no value with the next key's first 64; and 8 threads building streams concurrently match the serial sequences.
   - Red state: a compile failure on the same missing header, `common/sampling.h`, once built (syntax-checked against the stub only; not built, so not re-demonstrated this turn).
   - `ctest -N` now lists **612** tests, and `-L sampling` lists 15 (`DiskSampling` plus 14 new). The earlier 13-tests-of-8-executables figures in the red evidence above are as of the first run and are not re-measured.
   - The red evidence above (8 of 8 compile failures, 13 Not Run) was gathered before this change; the new test adds a ninth executable and a 14th Not Run test by the same cause.
