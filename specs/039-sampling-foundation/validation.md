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

### T018-T025: the sampler library, and the green run

Implemented on top of the committed tests (`2eeca56`); `nice -n 19`, `-j4`, one job at a time.

- `src/common/sampling.h` (T018, and the inline parts of T019-T022): `Backend`, `SampleId`, `SampleKey`, `SubstreamKey`, `Sample2D/3D/4D`, `CSamplingConfig`, the camera slots, hashing (`finalize64`, `mix`), the Owen machinery (`nestedUniformScramble`, a Laine-Karras permutation under bit reversal, after Burley 2020), Sobol dimensions 0 and 1, the Independent backend, `get1D/get2D/get3D/get4D`, `floorDiv/floorMod`, `cameraKey/cellKey/childKey/substream`, and `CKeyedStream`. All hot functions are `inline`; the header includes only `<bit>` and `<cstdint>`, and there is no mutable global state.
- `src/common/sampling.cpp`: the four camera-slot accessors and `sobolUnscrambled`.
- `src/common/samplingPmj02.cpp`: the PMJ02 tables (64 patterns x 4096 points, 2 MB), built under `std::call_once`, and `pmj02Get1D/2D`.
- `src/common/CMakeLists.txt` (T023): both `.cpp` files go into `openrendercommon_obj`, so they are in both the shared and the static `libRIBCommon`. The common headers are not installed (the glob only feeds the target's sources), so there is nothing to install.
- `tests/unit/sampling/CMakeLists.txt` (T024): `EXCLUDE_FROM_ALL` removed.

#### How the net property is kept (G3, G4)

- The sample index is scrambled per (sub-stream, dimension) with a nested uniform permutation, and the Sobol output is Owen-scrambled with another. A nested permutation of the index maps the first 2^m indices onto one aligned block of 2^m Sobol indices, and every aligned block of a (0,2)-sequence is a (0, m, 2)-net, so every prefix stays a net while the dimensions are decorrelated.
- **Deviation to note (fixed in the amendment below; see "Step 2"): the PMJ02 patterns were Owen-scrambled Sobol (0,2)-sequences, not the output of Christensen et al.'s PMJ02 generator.** I could not reconstruct that generator reliably, and the tests (G4) and the spec's stated property are met without it. The practical consequence is that the PMJ02 and Sobol backends are statistically the same family at power-of-two prefixes (see the discrepancies below). The comment in `samplingPmj02.cpp` says so. Whether a true PMJ02 generator is wanted is the maintainer's call.
- The camera accessors seed one sub-stream per pixel from the pixel's `path` and index it by the camera `sampleIndex`. The Morton ordering across pixels in research R2.1 is not applied.

#### Results

- Build: `cmake --build build --config Release -j4`, exit 0, no compiler warnings (13 s, incremental).
- `ctest -L "sampling|common|architecture"`: **50/50 passed**, including all 14 new `Sampling_*` tests and `Common_StateOwnership`.
- Full suite, `nice -n 19 ctest --test-dir build -j4 --output-on-failure`: **612/612 passed** (598 baseline tests + 14 new), 108 s. No failure, so no flaky re-run was needed. `Parity_transparency-matte-aov` (the one T003 flake) passed. Every one of the 598 T003 tests passes, so nothing changed under `Legacy`: no hider or shading code was touched.
- The files were then reformatted with the repo's `.clang-format` (its neighbours were clean); the rebuild and `-L "sampling|common|architecture"` were re-run after that: 50/50 passed. The full suite was not repeated after the whitespace-only change.
- Architecture: `ctest -L architecture` 21/21. `archcheck`: `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; the new `src/common` files classify as `Infra` (prefix `src/common/`) with `Infra refs 0`, and no allowlist line was added. The same three metric warnings as on master remain (Geometry +2, Hiders +5, TOTAL +7).

#### Measured values for the PAUSE 2 report

`Sampling_ConvergenceSmooth` (log-log RMSE slope of the smooth integrand, 32 keys, N = 2^2..2^12):

| Backend | Slope | Required |
|---|---|---|
| Sobol | -1.336 | <= -0.9 |
| PMJ02 | -1.303 | <= -0.9 |
| Independent | -0.504 | <= -0.4 |

`Sampling_Discrepancy` (exact 2D star discrepancy, mean of 8 keys):

| N | Sobol | PMJ02 | Independent |
|---|---|---|---|
| 64 | 0.04244 | 0.04283 | 0.14885 |
| 256 | 0.01325 | 0.01334 | 0.07969 |
| 1024 | 0.00381 | 0.00389 | 0.04151 |

`Sampling_Uniformity`: minimum p over the 16 dimensions is 1.0 for Sobol and PMJ02 (both are exactly stratified over 2^16 draws) and **0.0096 for Independent**. Per-check level 2.08e-4. The un-corrected p >= 0.01 would have failed the Independent backend here, which is the luck-of-the-key failure that the Bonferroni change removed.
`Sampling_NetProperty` and `Sampling_PMJ02Property`: 512 (key, dimension) pairs each, 0 violations.

### Amendment: true PMJ02 and Morton-ordered Sobol camera (tests first)

The maintainer asked for the two deviations above to be fixed inside Phase 2. This section records step 1 (tests, shown red). No generator or Morton code exists yet.

#### Tests written

| Test | File | Folded into | What it checks |
|---|---|---|---|
| `Sampling_PMJ02Construction` | `test_pmj02_table.cpp` | T011 | the 64 PMJ02 patterns follow the construction strategy of Christensen et al. 2018 (see below), are distinct, deterministic and thread-safe on first touch; a negative control runs the same checker on Sobol points |
| `Sampling_MortonIndex` | `test_morton.cpp` | T015 | `mortonIndex(px, py)` against 22 hand-computed values (zero, powers of two, 0x5555/0xAAAA patterns, negative pixels and the 65536 wrap), an independent bit-loop reference over a range crossing zero and the wrap, and the locality of aligned pixel groups (an aligned 2^s x 2^s group owns an aligned run of 4^s consecutive indices) |
| `Sampling_CameraSobolBlocks` | `test_morton.cpp` | T015 | for six sample counts (1x1, 2x2, 3x3, 3x4, 8x8, 1x2) the Sobol camera index is `mortonIndex·2^k + sampleIndex` with 2^k the power-of-two ceiling of nx·ny; all (pixel, sample) indices are distinct over a 16x16 pixel window that includes negative pixels; each pixel stays in its own block; PMJ02 and Independent index by the sample alone; an aligned group of pixels (2x2 pixels at 2x2 spp, 2x2 at 4x4, 8x8... in four size combinations, at positive and negative offsets) draws a (0, m, 2)-net of film samples and of lens samples and fills 2^m strata of time and of importance; the sequence is pure and changes with frame and seed |

New test-only API in `sampling.h`: `kPmj02Patterns`, `pmj02Point(pattern, j)` (the table point before the runtime shuffle and scramble), `mortonIndex`, `cameraSequenceIndex`. `pmj02Point` and `cameraSequenceIndex` (still returning the sample index, the current behaviour) are defined; `mortonIndex` is declared and deliberately not defined.

#### The PMJ02 discriminator, and how sound it is

- The net property does not separate PMJ02 from Owen-scrambled Sobol: every (0,2)-sequence has net prefixes, and a randomised Sobol sequence has the same joint distribution of pairwise interval structure as any other (0,m,2)-net. So the test does not rely on it (`Sampling_PMJ02Property` already checks it).
- What separates them is the generation strategy. The paper describes "a simple, diagonally alternating strategy that progressively fills in holes in increasingly fine stratifications", and Pharr 2019 (§2.2, §3) describes the same procedure: select an empty elementary interval for the next sample (Christensen et al.'s choice of interval), then place the sample in the admissible part of that interval. The test checks the consequences on the table in sequence order, for N = 2^k:
  - **locality**: sample N + i lies in the same floor(k/2)-digit cell (a cell of the sqrt(N) x sqrt(N) grid for even k) as sample i, for k = 0..11;
  - **diagonal**: for even k, sample N + i lies in the half of that cell opposite sample i in both x and y.
  A Sobol sequence breaks both (sample N + i has its first y digit flipped relative to sample i), and the test requires its checker to report violations on Sobol points, scrambled and unscrambled (4092 and 4082 locality violations per 4096 points; 1364 and 40 diagonal ones).
- **Confidence.** High that the discriminator is sound and not weak: it fails on the current Owen-scrambled-Sobol tables (4092 locality and 1364 diagonal violations in every one of the 64 patterns), and a reference generator I wrote in the scratchpad passes it on all 64 patterns (0 violations, every prefix a (0,m,2)-net, about 5 ms per pattern). Medium-high that it matches the paper letter for letter: the diagonal rule for even steps is the paper's stated strategy (confirmed by its abstract and a search summary of the supplement), and locality at odd steps is consistent with Pharr's description, but I could not read the paper itself or its supplement (the Pixar library URLs redirect to a landing page), so the odd-step rule (new sample in the same cell, adjacent sub-quadrant) is the part I am least sure the paper states in these words. If the maintainer prefers a narrower claim, restricting the locality check to even k loses nothing against Sobol.
- No pbrt-v4 or other code or tables were read or used. The scratch generator was written from the two papers' descriptions and my own search.

#### Red evidence

- Default build (`cmake --build build --config Release -j4`): exit 0. The two new targets are `EXCLUDE_FROM_ALL`.
- `test_sampling_pmj02_table` builds. `ctest -R Sampling_PMJ02Construction`: **Failed**. All 64 patterns report `range 0, net 0, locality 4092, diagonal 1364 violation(s)`: the current tables are Owen-scrambled Sobol.
- `test_sampling_morton` **fails to link**: `Undefined symbols ... "openrender::sampling::mortonIndex(int, int)"`. `ctest -R "Sampling_MortonIndex|Sampling_CameraSobolBlocks"`: both **Not Run**.
- To show `Sampling_CameraSobolBlocks` fails for the right reason and not only on the link error, a scratch build linked the test against a correct `mortonIndex` and the current library: `Sampling_MortonIndex` then passes (hand values confirmed), and `camera_sobol_blocks` fails 25316 checks: the index is not `morton·2^k + sampleIndex` (23460), pixels share blocks (1530, 6 of them with repeated indices), and the aligned-group film, lens, time and importance checks all fail (80 groups each).
- The tests were also run against a complete scratch implementation of both features (outside the repository, discarded): all three pass.
- The 14 earlier `Sampling_*` tests are untouched.

#### Revision after the test review (three changes)

The edits were first syntax-checked only (`-fsyntax-only`), while the machine was with spec 024; the red run below was made afterwards, on a free machine.

1. **Locality on even k only** (`Sampling_PMJ02Construction`). The locality check now covers only even k (N = 4^j), together with the diagonal rule. The odd-k locality count is kept as an informational print (`localityOdd`), with a comment that the odd step is unverified against the paper's text and may be enabled once someone confirms it from Christensen et al. 2018. The Sobol negative control still fails the narrower test, checked here with a Python mirror of the Sobol path (the C++ run is pending): unscrambled Sobol gives 1364 locality and 1364 diagonal violations over even k, and Owen-scrambled Sobol with three arbitrary seed triples gives 1352-1364 locality and 44-68 diagonal violations. Expected current-table result in the C++ run: every one of the 64 patterns fails with the even-k counts (locality 1364, diagonal 1364), since the tables are still scrambled Sobol.
2. **64-bit camera sequence index.** `detail::cameraSequenceIndex` returns `uint64_t` (`(uint64_t(mortonIndex) << k) + sampleIndex`), so the Morton term no longer wraps at 32 bits. I kept `mortonIndex` at 16 bits per axis and returning `uint32_t`, and **65536 pixels per axis is the stated limit** (negative and larger coordinates wrap modulo 65536); an 8K frame is 7680 x 4320, so the limit is not reached. The Sobol draw functions must therefore take a 64-bit index on the camera path (implementation step). New cases in `Sampling_CameraSobolBlocks`, on an 8K frame at 256 spp (16x16) and 4096 spp (64x64):
   - deliberate pixel pairs that collide under a 32-bit index (differing by 4096 px at 256 spp, 1024 px at 4096 spp, in x, in y, in both, and at the far corner), each asserted to be a genuine 32-bit collision (equal truncated index, different Morton index), whose 64-bit indices must differ and whose film samples must differ; the pairs were checked by hand-run arithmetic to be valid collisions inside the frame;
   - a scan of the whole frame for every pixel that collides with (100, 200) under a 32-bit index (there must be at least one), each of which must have a different 64-bit index;
   - a sparse lattice over the frame (every 11th pixel in x and 13th in y, two samples each), all indices distinct.
   The existing cases now compare 64-bit indices, and the 16x16-window check that each pixel owns its own block is exact (the old comment about the 32-bit wrap is gone).
3. **`detail` namespace.** Everything that is not the public API moved into `openrender::sampling::detail` in `sampling.h`: hashing (`finalize64`, `mix`, `toUnitFloat`), the Owen and Sobol machinery, the backend entry points (`sobolGet*`, `independentGet*`, `pmj02Get*`), `kPmj02Points`, `kPmj02Patterns`, `pmj02Point`, `mortonIndex`, `cameraSequenceIndex` and `sobolUnscrambled`. The public API is the contract's: `Backend`, the key and sample types, `CSamplingConfig`, `CameraSlot`, `cameraKey/cellKey/childKey/substream`, `get1D/2D/3D/4D`, `floorDiv/floorMod`, the camera accessors, and `CKeyedStream`. **I put `mortonIndex` in `detail` too**: only the camera accessors in `sampling.cpp` use it, the hiders compute a `SampleId` and call `cameraFilm` and the others. `floorDiv`/`floorMod` stay public because the hiders build `SampleId`s with them. The definitions in `sampling.cpp` and `samplingPmj02.cpp` moved with the declarations, and the tests that used these names now say `sp::detail::`. `contracts/sampler-api.md` notes where `sobolUnscrambled` lives.

#### Red evidence for the revised tests

`nice -n 19`, `-j4`, one job at a time, on the free machine.

- Default build (`cmake --build build --config Release -j4`): exit 0. This rebuilt the 14 earlier `Sampling_*` test executables against the `detail` move without any edit to their logic. The two amended targets are still `EXCLUDE_FROM_ALL`.
- `ctest -L sampling`: 18 tests, **15 passed, 3 failed**. The 14 earlier tests and `DiskSampling` pass (so the namespace move broke nothing). The three failures:
  - `Sampling_PMJ02Construction`: **Failed.** All 64 patterns report `range 0, net 0, locality 1364, diagonal 1364` (even k only; the tables are still scrambled Sobol), 87296 locality and 87296 diagonal violations in total. The odd-k locality count (174592) is printed for information and is not checked. The Sobol negative control passes the narrower test as the Python mirror predicted: unscrambled Sobol 1364 locality and 1364 diagonal violations, scrambled Sobol (the C++ run) 1362 and 40, both with 0 net violations.
  - `Sampling_MortonIndex` and `Sampling_CameraSobolBlocks`: **Not Run.** `test_sampling_morton` fails to link on `openrender::sampling::detail::mortonIndex(int, int)`.
- To show `Sampling_CameraSobolBlocks` fails for a real reason, a scratch build (discarded) linked `test_morton.cpp` against a correct stand-in `mortonIndex` and the current library. `Sampling_MortonIndex` then passes (hand values confirmed). `camera_sobol_blocks` fails: the index is not `morton·2^k + sampleIndex` (23460 checks), pixels share blocks (1530), the aligned-group film, lens, time and importance checks fail (80 groups each), and the new 8K cases fail too: the deliberate 32-bit-collision pairs get equal indices (24 pair checks `ia != ib`, plus the formula checks), the 8K collision scan finds 3 pixels colliding with (100, 200) at 256 spp and 39 at 4096 spp and their indices are not distinct (42 failures), and the lattice indices are not distinct (2 failures, one per sample count).
- The build and the tests left nothing running.


### Step 2 of the amendment: the implementation (T020, T021, T022 revised)

Run after the maintainer committed the tests; `nice -n 19`, `-j4`, one job at a time.

#### What was implemented

- **PMJ02 generator** (`src/common/samplingPmj02.cpp`), written from Christensen, Kensler and Kilpatrick 2018 and Pharr 2019; no pbrt-v4 or other code or tables were read or used. Each of the 64 patterns is built by doubling 2^k points to 2^(k+1), for k = 0..11: for an even k the new point for existing point i goes in the same cell and the diagonally opposite sub-quadrant; for an odd k in the same cell, in whichever sub-quadrant stays admissible. The exact digits are found by a depth-first search over the occupancy of the elementary intervals (one boolean each: Pharr's occupancy, searched by digits rather than by his binary trees), which checks every interval whose shape is fixed so far. The randomness is a counter-based hash of the pattern number, so the tables are identical on every run. One `std::call_once` per pattern, so only the patterns a render uses are built. A step that could not be completed would fall back to an Owen-scrambled Sobol sequence for that pattern; this has not occurred: a fallback pattern would fail the locality check, and `Sampling_PMJ02Construction` passes on all 64 (restarts of a step were not counted).
- **Runtime index shuffle for PMJ02 changed**, which the first run of the existing tests forced: with real PMJ02 patterns `Sampling_PMJ02Property` failed (14954 violations), because only the *prefixes* of a PMJ02 sequence are nets, and the Sobol-style shuffle moved the first 2^m draws onto another aligned block. The shuffle now permutes each dyadic block [2^t, 2^(t+1)) among itself (a nested uniform permutation of its t low bits), so every prefix maps onto itself. The approved test was not touched.
- **Morton-ordered Sobol camera**: `detail::mortonIndex` (16 bits per axis), `detail::cameraSequenceIndex` returning `uint64_t` (`(uint64_t(morton) << k) + sampleIndex`), and a 64-bit Sobol draw (`sobolGet1D64`/`sobolGet2D64`): the 64-bit index is scrambled by a nested uniform permutation built from two 32-bit ones (the low half's seed also depends on the whole high half), the first 32 digits of Sobol dimensions 0 and 1 are taken, and the Owen scramble is applied as before. The Sobol camera accessors seed one sequence per frame from the frame and the seed only (no pixel), so a pixel is a block of it; the PMJ02 and Independent camera accessors keep one sub-stream per pixel.
- `EXCLUDE_FROM_ALL` removed from `test_sampling_pmj02_table` and `test_sampling_morton`. The design stage's stated limit is 65536 pixels per axis (`mortonIndex` takes the low 16 bits of each coordinate).

#### Results

- Build: `cmake --build build --config Release -j4`, exit 0, no compiler warnings.
- `ctest -L sampling`: **18/18 passed** (the 14 original `Sampling_*` tests, `Sampling_PMJ02Construction`, `Sampling_MortonIndex`, `Sampling_CameraSobolBlocks`, and `DiskSampling`). No approved test was edited, and no threshold or timeout was changed.
- Full suite, `nice -n 19 ctest --test-dir build -j4 --output-on-failure`: **615/615 passed** (598 baseline + 14 + 3 new), 111 s real time. No failure, so no flaky re-run. `Visual_camera-dof-reyes` and `Parity_transparency-matte-aov` both passed. Nothing changed under `Legacy`: no hider or shading file is touched.
- Architecture: `ctest -L architecture` 21/21; `archcheck` `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`, `Infra refs 0`, no allowlist line, and the same three metric warnings that master prints (Geometry +2, Hiders +5, TOTAL +7).
- New files are clang-format clean.

#### Numbers for the PAUSE 2 report (all three backends, PMJ02 now real)

`Sampling_ConvergenceSmooth` (log-log RMSE slope of the smooth integrand, 32 keys, N = 2^2..2^12):

| Backend | Slope | Required |
|---|---|---|
| Sobol | -1.336 | <= -0.9 |
| PMJ02 | -1.325 | <= -0.9 |
| Independent | -0.504 | <= -0.4 |

`Sampling_Discrepancy` (exact 2D star discrepancy, mean of 8 keys):

| N | Sobol | PMJ02 | Independent |
|---|---|---|---|
| 64 | 0.04244 | 0.04257 | 0.14885 |
| 256 | 0.01325 | 0.01293 | 0.07969 |
| 1024 | 0.00381 | 0.00378 | 0.04151 |

(With the earlier Sobol-derived tables PMJ02 was 0.04283 / 0.01334 / 0.00389 and -1.303: the real generator is slightly better at N = 256 and 1024, as the paper reports for non-trivial prefixes, and the two remain within a few percent of each other.)

- `Sampling_Uniformity`: minimum p over 16 dimensions is 1 for Sobol and PMJ02 and 0.0096 for Independent (per-check level 2.08e-4).
- `Sampling_NetProperty` / `Sampling_PMJ02Property`: 512 (key, dimension) pairs each, 0 violations.
- `Sampling_PMJ02Construction`: 64 patterns, 0 locality and 0 diagonal violations at even k, and 0 odd-k locality violations (informational, not checked). The Sobol control still fails the narrower test (1364 / 1364 unscrambled, 1362 / 40 scrambled).
- `Sampling_CameraSobolBlocks` on the 8K frame: 3 pixels collide with (100, 200) under a 32-bit index at 256 spp and 39 at 4096 spp; every one keeps a distinct 64-bit index.
- **PMJ02 table generation time**: 64 patterns built on first touch in 57-88 ms in total (three runs: 87.5, 57.4 and 57.0 ms; the first includes cold-start effects), about 0.9 ms per pattern (maximum 0.96 ms warm, 2.7 ms cold). Each pattern is built only if a render draws from it.

## Phase 3: scene option and config injection, tests first (T026-T030)

Run 2026-10-16, `nice -n 19`, `-j4`, one job at a time. No implementation: no token, declaration, parse branch, RIB writer branch or `CSamplingConfig` injection exists yet.

### Tests written

| Task | Test | File | What it checks |
|---|---|---|---|
| T026 | `SamplingOption_Parse` | `tests/unit/rib_parse/test_sampling_option.cpp` (case `parse`) | default `Legacy`, seed 0; `"sampler"` set to `pmj02`, `independent`, `sobol` (and the internal `legacy`) sets the backend; `"seed" [-7]` is stored as `uint32` 4294967289; setting one leaves the other alone; both values read back through `COptions::find()` (the lookup RSL `option()` uses): the sampler as a string, the seed as an integer (-7) |
| T027 | `SamplingOption_UnknownName` | same file (case `unknown_name`) | `["foo"]` produces exactly one diagnostic, with code `RIE_BADTOKEN` (the mapping of `CODE_BADTOKEN`; never `RIE_LOG`), severity error or warning, containing `Unknown sampler "foo"` and `sobol, pmj02, independent`; the backend stays at the default; a later valid name still works and adds no diagnostic |
| T028 | `SamplingOption_RibRoundTrip` | `tests/unit/rib_parse/test_sampling_option_roundtrip.cpp` | in process, with three consecutive sessions in one process: (1) the parser reads a RIB with both tokens, spelled pre-declared (`"sampler"`, `"seed"`) and with inline declarations (`"string sampler"`, `"int seed"`), and stores equal options; (2) the same RIB is parsed straight into the RIB writer (`RiBegin("file")` then `ribParse()`), and the output contains `Option "sampling"`, `sampler`, `pmj02`, `seed` and `-7`; (3) the written file read back gives options equal to the original, and writing the written file again gives identical text. A `Option "limits" "bucketsize" [16 16]` control line travels the same path |
| T029 | `Common_VocabularyConsistency` | existing, unchanged | see below |

Both new executables link `ribRender` and `openrendercommon`, run with `ORENDERHOME` at the source root (no `.orenderrc`), and are registered with labels `rib_parse;unit;sampling`. They need no fixture files: the RIBs are written to a temporary directory by the test.

Test-support change in `src/ri/state/options.{h,cpp}` (needed for the tests to compile, and the only source change in this step): `COptions` gains `samplingBackend` (default `Backend::Legacy`) and `samplingSeed` (default 0), and `options.h` includes `common/sampling.h` (Infra). Nothing reads or sets them yet.

### T029: the vocabulary check

No token exists yet: `RI_SAMPLING`, `RI_SAMPLER` and `RI_SEED` are all absent from the three files, so T031 adds all three. To show that `Common_VocabularyConsistency` (`vocabulary_consistency.cmake`) catches a token missing from any of the three lists, the script was run against copies of the three files in the scratchpad with `RI_SAMPLING` added, so the repository files were never touched and need no revert:

| `RI_SAMPLING` added to | Result |
|---|---|
| none | agree on 235 tokens |
| `riTokens.cpp` only | error: `in definitions but not public: RI_SAMPLING` |
| `riVocabulary.h` only | error: `in vocabulary but not public: RI_SAMPLING` |
| `ri.h` only | error: `in public but not vocabulary` and `in public but not definitions` |
| `riTokens.cpp` + `riVocabulary.h` | error: `in vocabulary but not public`, `in definitions but not public` |
| all three | agree on 236 tokens |

### Red evidence

- Default build (`cmake --build build --config Release -j4`): exit 0. The new `COptions` fields rebuilt the renderer, and the two new executables compile and link.
- `ctest -R "SamplingOption_|Common_Vocab"`: 5 tests, **3 failed**, 2 passed (the two vocabulary tests, unchanged):
  - `SamplingOption_Parse`: **Failed**, 11 checks. Every `RiOption("sampling", ...)` reports `Unknown option: "sampling"`, so the backend stays `Legacy` and the seed 0 (default-state checks pass).
  - `SamplingOption_UnknownName`: **Failed**, 4 checks. The one diagnostic is `Unknown option: "sampling"` (code 41, severity error) rather than `Unknown sampler "foo" ...`, and the later valid name is not applied.
  - `SamplingOption_RibRoundTrip`: **Failed**, 12 checks. The parser reports `Parameter "sampler" is not declared` and `Parameter "seed" is not declared` for the pre-declared spelling and `Unknown option: "sampling"` for the inline one. The RIB writer output contains only the control line (`Option "limits" "bucketsize" [16 16]`), which shows that the parser-to-writer path itself works and the failures are confined to the sampling tokens.
- `ctest -L architecture`: 21/21 passed with the new `options.h` include.

### T031-T037: implementation and the end-of-phase run

`nice -n 19`, `-j4`, one job at a time. The approved tests were not touched.

#### What was implemented

- **Tokens** (T031): `RI_SAMPLING` ("sampling"), `RI_SAMPLER` ("sampler") and `RI_SEED` ("seed") in `src/ri/parse/ri.h`, `src/common/riVocabulary.h` and `src/common/riTokens.cpp`. None existed before. `Common_VocabularyConsistency` agrees on 238 tokens.
- **Pre-declaration** (T032): `"sampler"` as `string` and `"seed"` as `int` in `initDeclarations()` (`rendererDeclarations.cpp`).
- **Storage and query** (T033): `COptions::samplingBackend` (default `Legacy`) and `samplingSeed` (default 0; added with the tests), and `COptions::find()` answers category `sampling`: `sampler` as a string (`sobol`, `pmj02`, `independent`, `legacy`) and `seed` as an integer. Two small public helpers in the sampler library map names and backends: `samplerName(Backend)` and `samplerFromName(const char *, Backend &)` (not `backendName`: the approved test utilities already define `sptest::backendName`, which made the call ambiguous through `using namespace sptest`).
- **Parsing** (T034): a `"sampling"` branch in `RiOptionV()`. An unknown sampler name raises `error(CODE_BADTOKEN, "Unknown sampler \"foo\" (valid: sobol, pmj02, independent); using the default sampler")` and keeps the default. The seed is any integer, reinterpreted as `uint32`. The message says "the default sampler" and not `"sobol"` because the internal default is still `Legacy` until Phase 9. A token declared inline (`"string sampler"`) is re-issued under its plain name, as the other option groups do.
- **RIB output** (T035): `CRibOut::RiOptionV()` writes `Option "sampling" "sampler" ["name"]` and `Option "sampling" "seed" [n]`.
- **Config injection** (T036): `CRenderer::beginFrame()` builds one immutable `CSamplingConfig {backend, seed, frame, pixelXsamples, pixelYsamples, jitter}` from the frame's options and calls `setSamplingConfig()` on every context it creates (raytrace, reyes, zbuffer, photon, and the default reyes), before `updateState()`. `CShadingContext` (`libshader/shading/shading.h`) has a protected `sampling` member, `Legacy` until set, with `setSamplingConfig()` and `samplingConfig()`. **Gate:** no hider or shading file reads a new `CRenderer::` symbol (the diff of `src/ri/hiders` and `src/libshader` contains no `CRenderer::` token); the core reads only what it already read. `shading.h` includes `common/sampling.h` (Infra).

#### Results

- Build: exit 0, no compiler warnings.
- `SamplingOption_Parse`, `SamplingOption_UnknownName`, `SamplingOption_RibRoundTrip`, `Common_*` and `Sampling_*` (34 tests): **34/34 passed**.
- Full suite, `nice -n 19 ctest --test-dir build -j4 --output-on-failure`: **618 tests** (615 + the 3 new), and the baseline holds:
  - first run: **617/618**, one failure, `Visual_sphere-shader-parameter-reyes-slo`: `3 block(s) exceed threshold 20. Worst avg diff=77.23 on channel G at block (34,28)`, `MaxBlockAvgDiff 77.23`, `FailBlocks 3` (the unexplained flake that failed once on spec 024). Re-run in isolation: **60 of 60 passed** (`--repeat until-fail:60`) and five `-V` runs gave MaxBlockAvgDiff 6.06, 5.97, 6.81, 6.81 and 6.86 for the `-slo` variant (4.88, 6.83, 0.23, 5.97 and 6.78 for the plain one, so the scene's own run-to-run diff varies a lot, 0.23 to 6.8, which fits a source of nondeterminism in this tiny scene).
  - second full run: **618/618 passed**, including that test.
  - The change cannot reach a render: under `Legacy` nothing reads the config. The sampling-config injection changes no output.
- Architecture: `ctest -L architecture` 21/21; `archcheck` `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`, `Infra refs 0`, `Shading refs 9` (unchanged), no allowlist line, and the same three metric warnings as master.

#### Unexpected

- **RSL `option()` is not reachable through `COptions::find()`.** T033 says the options are queryable through `COptions::find()` and RSL `option()`. `find()` is done and tested, but it has no callers outside the tests: RSL's `option()` (`CShadingContext::options()` in `shading.cpp`) answers a fixed list of RISpec names through `CRendererServices` virtuals, and a custom name such as `sampler` would need a new virtual on that Shading-to-Core interface. I did not widen that interface; whether to is the maintainer's call (it matters for specs 026/029).

### T037a: RSL `option()` for the sampling options, tests first

Added after Phase 3: the maintainer decided that RSL `option()` must reach the sampling options inside spec 039 (completing the RSL half of T033). No implementation yet.

#### Naming convention

RSL option names today are the RISpec names (`"Format"`, `"PixelSamples"`, `"Gamma"`, ...) matched in `CShadingContext::options()` (`shading.cpp`), plus the `user:` prefix, where the part after the colon is looked up in the user options. The new queries follow the prefix style: `"sampling:sampler"` and `"sampling:seed"`. They are answered through `CRendererServices`, the Shading-to-Core interface, so `src/libshader` gains no `CRenderer::` reference. The tree has no `recording_services.h` test double; `CRendererServicesImpl` (`src/ri/render/rendererServicesImpl.h`) is the only `CRendererServices` implementer.

#### Tests written

| Test | File | What it checks |
|---|---|---|
| `SamplingOption_RslQuery` | `tests/unit/rib_parse/test_sampling_option_rsl.cpp` (case `query`) | on a real shading context via `CShadingContext::queryOption()` (the call RSL `option()` makes under both the interpreter and the JIT): defaults give `"legacy"` and 0; a scene with `"sampler" ["pmj02"]` and `"seed" [-7]` gives `"pmj02"` and -7.0; `sampling:foo`, `sampling:` and a bare `sampler` are not found; `PixelSamples` still answers (control) |
| `SamplingOption_RslQueryNoServices` | same file (case `no_services`) | with the shading state's services set to null every name, including `PixelSamples`, is "not found"; with the services restored the answers return |
| `SamplingOption_RslQuery_{pmj02,default}_{rslo,slo}` | `tests/visual/CMakeLists.txt`, driver `tests/visual/test_pixel_probe.cpp`, shader `shaders/sampling_option_probe.sl`, scenes `examples/rib/tests/sampling-option-*.rib` | the probe shader encodes `option()`'s results in its colour (R 0 not found / 0.5 found, not `pmj02` / 1 found, `pmj02`; G 1 if the seed was found; B = (seed + 1000) / 2000) and the driver reads the centre pixel of a float TIFF back to 1e-5, under the interpreter (`.rslo`) and the LLVM JIT (`.slo`): pmj02 with seed -7 expects (1, 1, 0.4965), and no option expects (0.5, 1, 0.5) |

The probe uses the varying-flag-in-`if` pattern the other probe shaders use for string comparison (GitHub issues #6 and #8). The driver and the four scenes are new; the shader adds a 106th compiled shader. `test_sampling_option_rsl` links `libshader_shading` as well, since it calls the shading context directly.

#### Red evidence

- The default build exits 0.
- `ctest -R SamplingOption_Rsl`: **6 of 6 failed**, each for the intended reason:
  - the four probe renders read back R 0, G 0 and B 6.6725 (`seed` stays at its default 12345 because the query found nothing), against the expected (1, 1, 0.4965) for pmj02 and (0.5, 1, 0.5) for the defaults, under both `.rslo` and `.slo`;
  - `SamplingOption_RslQuery`: 4 checks fail (the sampler and seed, for the defaults and for the set values); the unknown-name checks and the `PixelSamples` control pass;
  - `SamplingOption_RslQueryNoServices`: 2 checks fail (the query with services returns "not found"); the checks that the query is "not found" without services already hold, since the existing code returns false for a null `services`.

### T037b and T037c: RSL `option()` implemented, and the run

`nice -n 19`, `-j4`, one job at a time; no approved test changed.

- **Interface** (`src/libshader/shading/rendererServices.h`): two new pure virtuals on `CRendererServices`, `const char *samplingSampler() const` (a name with static storage) and `int samplingSeed() const` (the signed integer the scene gave).
- **Core** (`rendererServicesImpl.h`, the only implementer in this tree): both read two new `CRenderer` statics, `samplingBackend` and `samplingSeed`, which `copyOptions()` fills from the frame's options (definitions in `rendererStatics.cpp`). Core already reads `CRenderer::` there; the statics are not read by `src/libshader` or the hiders.
- **Shading** (`shading.cpp`, `CShadingContext::options()`): `"sampling:sampler"` writes the name to a `char *` destination and `"sampling:seed"` writes the seed as a float. With no services installed the function already returned "not found" at its top, so the no-services case needed no code.
- No `CRenderer::` token is added under `src/libshader` (`git diff HEAD -- src/libshader | grep -c "CRenderer::"` is 0).

#### Results

- Build: exit 0, no compiler warnings.
- `SamplingOption_*` (9 tests): **9/9 passed**, including the six new ones. Under both the interpreter and the JIT the probe reads back (1, 1, 0.4965) for pmj02 with seed -7 and (0.5, 1, 0.5) for the defaults.
- Full suite, `nice -n 19 ctest --test-dir build -j4 --output-on-failure`: **624 tests** (618 + the 6 new).
  - First run: 622/624. `ShaderCompilerImmutability` failed with `1 missing reference(s)`: it compiles every shader in `shaders/` and diffs it against `tests/shaders/references/`, and the new probe shader had no reference. That was my omission when the shader was added with the T037a tests (the full suite was not run at that step). Fixed by adding `tests/shaders/references/sampling_option_probe.rslo`, the compiled output of the new shader (a new file; no existing reference or test was changed); the test and `SloinfoGoldenOutput` then pass.
  - The other failure, `Parity_transparency-matte-aov` (known flake): `1 block(s) exceed threshold 25.00. Worst avg diff=25.52`; in isolation six runs gave MaxBlockAvgDiff 23.86, 23.88, 23.86, 23.91, 23.88 and 23.91 (threshold 25.00).
  - Second full run: **624/624 passed**.
- Architecture: `ctest -L architecture` 21/21; `archcheck` `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`, `Shading refs 9` (unchanged), no allowlist line, and the same three metric warnings as master.

#### Merge interaction with spec 024 (not yet merged)

- 024 adds `tests/.../recording_services.h`, a `CRendererServices` test double. It will need overrides of the two new pure virtuals, `samplingSampler()` and `samplingSeed()`; until it has them it will not compile against this change.
- 024's comment in `shaders/CMakeLists.txt` says "107 shaders". The probe shader (`sampling_option_probe.sl`) adds one, so the count after the merge is 108 (this tree now compiles 106 shaders).
- The new reference `tests/shaders/references/sampling_option_probe.rslo` is a file 024 does not touch, so it merges cleanly; if 024 adds its own shaders it will add its own references.

## Branch upkeep: master (with spec 024) merged into 039, 2026-10-16

This is branch upkeep, not a `tasks.md` task: spec 024 was merged into master (a fast-forward to `b6981b1`), and master
was then merged into this branch, so Phases 4–9 build on the code they will ship with.

- **Textual conflicts:** only `.specify/feature.json`, resolved to this spec's value (`specs/039-sampling-foundation`).
  The seven files both branches touch (`rendererServices.h`, `shading.{h,cpp}`, `renderer.{h,cpp}`,
  `rendererServicesImpl.h`, `tests/visual/CMakeLists.txt`) merged automatically.
- **Semantic fix (the merge would not compile without it):** 024's two `CRendererServices` test doubles in
  `tests/shading_parity/recording_services.h` gained the two pure virtuals added in T037b.
  - `CRecordingServices` forwards `samplingSampler()` and `samplingSeed()` to `inner`, like every other lookup.
  - `CStubServices` answers `"legacy"` and 0, the renderer's defaults.
- **Shader count:** 024's two shaders plus this spec's probe make 108 (`shaders/*.sl`; the build prints "108 .rslo
  shaders compiled" and "108 .slo shaders compiled"). Updated in `shaders/CMakeLists.txt` (108 shaders, 216 objects;
  the ~1.4s timing was measured at 214 and is left as is) and in `CLAUDE.md`. 024's own validation records keep 107,
  the number true when they were written.
- **Build:** `nice -n 19 cmake --build build -j4`, exit 0, no compiler warnings (only the existing `ld` duplicate-library
  and orender-wire deployment-target notes).
- **Architecture:** `ctest -L architecture` 21/21; `archcheck` `RESULT PASS violations=0 stale=0 badid=0
  failing-cycles=0 notbuilt=0`, `TOTAL refs 740 (baseline 740) distinct 113 (baseline 113)`.
- **Full suite:** `nice -n 19 ctest -j4`, **635/635 passed** on the first run. That is 024's 609 plus 039's 26 new
  tests (624 − 598 = 26).

## Phase 4: identity plumbing, tests first (T038-T043)

Run 2026-10-17 on top of the merge (`988f9a9`), every build and test through the shared CPU lock helper (`cpujob.sh 039 ...`, `-j4`). No production code was added: the targets are `EXCLUDE_FROM_ALL`.

### Tests written (16 ctest tests in 4 executables)

| Task | Test(s) | Files | What it checks |
|---|---|---|---|
| T038 | `Sampling_ObjectIds_{scene_loads, instances, delayed, tessellation, splits}` | `tests/unit/sampling/test_object_ids.cpp`, `scene_probe.h` | `CObject::stableId` is set, non-zero, distinct, and identical across two loads and across 1 and 4 threads; two `ObjectInstance`s are two objects (one delayed-instance object each) whose render-time copies are `mix(instance, k)` and four different identities; a `DelayedReadArchive`'s children are `mix(parent, k)`; a polygon mesh's and a patch mesh's children are `mix(parent, childIndex)`; the pieces dicing splits a patch into (grid size 8) keep the patch's identity |
| T039 | `LibShader_CallSiteOrdinals` | `src/libshader/tests/test_call_sites.cpp` | one shader with two `random()` calls in a loop, a `trace`, a `gather` and an `illuminance`, compiled with the real `oshader` to `.rslo` and with `--jit` to `.slo`. The ordinal is the LAST operand of each stochastic instruction in the `.rslo` text, and the LAST argument (an `i32` constant) of each call to a `_v2` entry in the `.slo` bitcode (read with the LLVM API). Expected, in IR order: random 0, random 1, trace 2, gather 3, illuminance 4, identical in both |
| T040 | `LibShader_RandomNotPure` | same file | after the IR passes two identical `random()` calls are still two instructions, and a `random()` in a loop stays inside it |
| T041 | `LibShader_StaleShaderRejected`, `LibShader_StaleShaderRejectedJit` | `src/libshader/tests/test_stale_shaders.cpp` | a `.rslo` carrying the pre-039 format version (1.0.0) is refused by `parseShader` with a diagnostic naming the version, while a fresh one loads; the emitted `.slo` calls `op_random_f_v2`/`op_random_v_v2` and no unversioned entry, and a module renamed back to the old names fails at JIT bind with an unresolved-symbol diagnostic while the fresh one binds. Fixtures are generated in a temporary directory |
| T042 | `Sampling_CellKeyUniqueness_{patch, mesh, quadric, nurbs, subdivision, points, curves}` | `tests/unit/sampling/test_cell_keys.cpp`, `scene_probe.h` | per family, a small scene under `sobol`: keys equal exactly when (object, u, v) are equal (an edge shaded twice keeps one key, two different points never share one), and for points, where `sample()` writes no (u, v), every point shaded has its own key |

### Choices for approval

1. **The key probe is an in-process hook, not an AOV.** tasks.md T042 says "a probe shader that writes its point key to an AOV". No shading-language accessor for a point key exists, and `random()` does not draw from the keys until Phase 6, so an image-based probe would pass on the legacy generator and prove nothing. Instead the tests use a test hook the implementation adds (T049/T050): `CShadingContext::pointKeyObserver`, a null-in-production function pointer called once per shaded grid with `{surface, numVertices, keys, u, v}`. Its exact declaration is in `scene_probe.h`. Pixel-based checks would also be wrong on their own terms (a micropolygon covers several pixels; grid edges are shaded twice at identical (u, v)). Keys are filled only when the sampler is not `Legacy`, so the scenes ask for `sobol`; under `Legacy` nothing is computed.
2. **Pinned formulas.** A child's identity is exactly `openrender::sampling::detail::mix(parent.stableId, k)`; for tessellation children `k` is the child's index in creation order, compared as a set because the sibling chain may be in either order. The identity of a top-level object (`mix(scope, ordinal)`) is not pinned by value: the tests require it reproducible, independent of the thread count, distinct and non-zero.
3. **Instances.** An `ObjectInstance` adds one delayed-instance object (not its copies), whose children are the copies, made at render time. The tests are written for that shape (found by running the first draft against a stand-in; the first draft assumed the copies were added directly).
4. **Illuminance in J1.** contracts/shading-runtime.md J1 lists the entries that gain the `callSite` argument (`random`, the trace family, gather, occlusion, indirectdiffuse, texture family) but not illuminance, while C1 lists "light iteration in `illuminance`/`solar`/`illuminate`" and tasks.md T039 includes an `illuminance` in the shader whose lists must be identical. The test requires the JIT's `op_illuminance_begin` to be a `_v2` entry with a trailing `callSite` constant. If illuminance is not meant to carry one, T039 and C1 need to say so.
5. **T041 in a second file.** T039/T040 are in `test_call_sites.cpp` (compiler only); T041 needs the renderer (`parseShader`) and the JIT engine, which that executable must not link, so it is `test_stale_shaders.cpp`. The `.slo` half also pins an ABI consequence: the unversioned `op_random_f`/`op_random_v` must no longer be exported (T054), or a stale module would still bind to them.
6. **Reference regeneration (a decision needed now).** T052 bumps the `.rslo` format version and adds an operand to stochastic instructions. That changes the compiled output of every shader, so `ShaderCompilerImmutability` will fail against the ~106 checked-in `tests/shaders/references/*.rslo`, and the reference `.rslo` fixture of `SloinfoGoldenOutput` will no longer load. The documented workflow is to regenerate and commit the references. That edits approved, checked-in files; **I will regenerate them as part of T052/T053 only with your agreement.**
7. **Principle VIII.** The libshader tests are in `src/libshader/tests/` (class `Consumer`); `archcheck` reports their includes (including `ri.h` and `shader.h`) as Consumer edges with 0 violations and unchanged totals (740/113).

### Red evidence

- Default build: exit 0 (the four test targets are `EXCLUDE_FROM_ALL`). `ctest -N` lists **651** tests (635 + 16).
- `test_sampling_object_ids` and `test_sampling_cell_keys` **fail to compile**: `CPointKeyReport` and `CShadingContext::pointKeyObserver` do not exist, and `CObject::stableId` does not exist (9 and 3 errors respectively). Tests that depend on features not yet written are red by compilation, as in Phase 2.
- To check that these two are well formed beyond the missing symbols, I built them against a temporary stand-in (three lines: `stableId` in `CObject`, the hook declaration and its definition), then reverted it (`git status` clean for those files). With the stand-in all 12 cases compile, link and run through real Ri sessions (a render in each, `file` display), and fail for the intended reasons: `scene_loads` finds only zero identities; `instances` finds one object per instance and two copies each but wrong identities; `delayed` finds the three expanded children but not `mix(parent, k)`; `tessellation` finds the children but not `mix(parent, childIndex)`; `splits` and all seven cell-key cases see no hook call, so `shaded > 0` fails. **Not verified by that run:** the split-piece count (needs the hook), the points count (`shaded == 40`) and the uniqueness rules themselves, which need the implementation. The first stand-in run also found that the harness needed `DISPLAYS` in the environment (fixed) and the instance shape in item 3.
- `ctest -R "LibShader_(CallSite|RandomNot|StaleShader)"`: 4 tests, **3 failed**, 1 passed:
  - `LibShader_CallSiteOrdinals`: Failed. The `.rslo` stochastic instructions end in no ordinal (the last token is not the expected number) and the `.slo` has no `_v2` calls.
  - `LibShader_StaleShaderRejected`: Failed. The stale `.rslo` (version 1.0.0, today's version) loads, no diagnostic; the fresh one loads (control passes).
  - `LibShader_StaleShaderRejectedJit`: Failed on the precondition: the emitted module has 0 versioned and 2 unversioned `random()` calls.
  - `LibShader_RandomNotPure`: **Passed today**, a characterization result, not contorted: two identical `random()` calls compile to two instructions, and a `random()` in a loop stays at position 6 of a loop spanning 2..8. A uniform destination for `random()` does not compile at all (the test skips that variant and says so). The test stays as a regression guard for T052's pass changes.
- `ctest -L architecture`: 21/21.

### Phase 4 implementation (T044-T056)

Every build, ctest and render went through `cpujob.sh 039` at `-j4`. No approved test was edited.

#### What was implemented

- **Stable identity** (T044-T046): `CObject::stableId` (0 until assigned). A top-level object gets `mix(mix(0x5C09E1D, frame), ordinal)` in `CRendererContext::addObject`, the ordinal counting the world block's top-level objects from `RiWorldBegin`. Every other identity is assigned in one place, `CObject::setChildren`, to children that have none: `mix(parent, k)`, k being the position in the chain handed over (tessellation children, instance copies, procedural expansions, CSG leaves, patch-mesh patches), or the parent's own identity for split pieces (`inheritId`, used by `CTesselationPatch`). `cluster()` dummies get `mix(parent, 0|1)`. Children that already have an identity (top-level objects under the root) are untouched. This replaces the "ID-scope stack" of T045: because every expansion funnels through `setChildren`, no scope needs pushing, and nothing depends on creation order at render time or on `getGlobalID`.
- **Points** (T047): the whole `CPoints` object is shaded in one call, so the vertex index is the point's identity. Instead of making `CPoints::sample()` write an `elementId`, `fillPointKeys()` uses the vertex index as `elementId` for every `SHADING_0D` call (and `u = v = 0`); same semantics, no change to the primitive.
- **`CRay`** (T048): `key` and `depth` declared (Infra); set in Phase 5.
- **`CShadingState`** (T049): `pointKeys` (capacity 3 x maxGridSize), `batchKey`, `pointKeysFromRays`, `callSiteOccurrence` (declared, null until Phase 6 draws from it), `currentShaderKey` (declared; `execute()` sets it in Phase 6).
- **`shade()`** (T050): when the sampler is not `Legacy` and the call is a real shading pass, `PARAMETER_U|V` are requested, and after sampling `fillPointKeys()` fills `pointKeys` with `cellKey(object->stableId, u, v, elementId, frame, seed)`, sets `batchKey` from the grid's parametric origin, copies the real vertices' keys onto the derivative-offset tail vertices, and calls the test hook. Under `Legacy` none of this runs. `object->stableId` is an inline member read through the existing include.
- **`trace.cpp`** (T051): before `object->shade()` the shading-group loop copies `ray->key` into `pointKeys`, sets `batchKey = mix(bucket, stableId, depth, chunkIndex)` and flags the keys as supplied by the hider so `shade()` does not recompute them. `CRay::key` is not yet initialised by the ray-spawning code, so raytraced keys are meaningless until Phase 5 (and unused: Legacy only).
- **Compiler** (T052): `IRInstr::callSite` and a new last pass, `CCallSitePass`, number the stochastic calls of the final IR from 0 (Init section first): `random`, `urandom`, `trace`, `transmission`, `visibility`, `gatherHeader`, `occlusion`, `indirectdiffuse`, `illuminance`, `texture`, `environment`, `shadow`. `random`/`urandom` stay un-merged and un-hoisted (the pass never removes an instruction). `.rslo` format **1.0 -> 1.1** (`src/common/rsloFormat.h`): the version line, and the ordinal as the last operand of each such instruction.
- **Loader** (T053): both lexers compare the file's major.minor with the format version and refuse an older file with `Version mismatch`, now naming the format. The shading loader strips the trailing ordinal before it resolves the opcode (so the optional parameter pairs of `trace()` and friends and fixed-arity opcodes see the arguments they always did) and keeps it in `TCode::callSite` (it reuses two padding bytes; the struct size is unchanged). Pass 2 stores the constant before it can know it is an ordinal, so the constant tables stay sized as pass 1 counted them and only the argument slot is dropped.
- **JIT** (T054): the fifteen entries `op_random_{f,v}`, `op_texture_{f,c}`, `op_environment_{f,c}`, `op_shadow_f`, `op_visibility`, `op_transmission`, `op_trace_{f,c}`, `op_occlusion`, `op_indirectdiffuse`, `op_gatherHeader`, `op_illuminance_begin` are renamed `..._v2` and gain a trailing `uint32_t callSite`; the emitter passes the ordinal as an `i32` constant. The old names no longer exist, so a stale `.slo` fails at JIT bind (`Symbols not found: [ _op_random_f ]`). The new argument is ignored under `Legacy`. `solar` and `illuminate` carry no ordinal (contract J1a).
- `EXCLUDE_FROM_ALL` removed from the Phase 4 tests (T055). Docs: `contracts/shading-runtime.md` (J1 now lists illuminance, J1a, I1, the test hook), `research.md` R3.3 (hook instead of AOV), `tasks.md` T042.

#### Results (T056)

- Build: exit 0, no compiler warnings; this rebuilt every shader (108 `.rslo`, 108 `.slo`). T004's stat check: no compiled shader is older than `oshader` (0 of 216).
- Full suite: **651 tests, 649 passed, 2 failed**; no baseline test fails (the 635 tests of the merge baseline all pass, including the `-slo` suite, so the `_v2` symbols resolve at bind time and `addProcessSymbol()` is not needed). `ctest -L architecture` 21/21; `archcheck` `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`, TOTAL refs 740/113 (unchanged), no allowlist line. `Visual_camera-dof-reyes` failed once in an earlier full run and passes in isolation (16.97, threshold 20). The new tests: 14 of 16 pass (below).
- Mid-way the first full run had 57 failures: a real crash I introduced (the loader wrote the ordinal constant into a constant table that pass 1 had not sized when the ordinal was a shader's only constant) plus the expected reference failures. Fixed as described under the loader; the interpreter crashed on every shader of that shape.

#### The two failing new tests are defects in the approved tests (stopped, not edited)

1. **`Sampling_ObjectIds_splits`** cannot pass as written, in two ways, and the implementation behaves correctly. Evidence from a temporary probe (reverted): the 4x4 bicubic patch with `gridsize 8` produced **16 grids**, all shaded through **one surface** whose identity is `mix(patch, 0)` (`ad573a70c46cd3fd`), the identity of the mesh's only child.
   - `CHECK(pieces.size() > 1)` counts distinct `CSurface` pointers, but `CPatch::dice` splits into lightweight `CPatch` views and calls `drawGrid(object, ...)` with the one underlying surface, so every piece reports the same surface.
   - `CHECK(*ids.begin() == patch[0]->stableId)` expects the shaded surface to be the object `RiPatch` added, but `RiPatch` adds a patch *mesh* whose child patch is what is shaded, so the identity is `mix(mesh, 0)`.
   - Proposed fix (one test, your call): count grids instead (`reports().size() > 1`) and compare with `mixChild(patch[0]->stableId, 0)`.
2. **`LibShader_StaleShaderRejectedJit`**: the behaviour holds (the stale module fails to bind, `compileShader` returns null, `JIT session error: Symbols not found: [ _op_random_f ]`), but the last check expects that text in a diagnostic captured through the diagnostic sink, and the JIT engine reports the failure through the logging layer / LLVM's session reporter, which the sink never sees. Proposed fix: capture the message from the logger or drop the text check and keep `entry == nullptr`.

#### Reference regeneration (approved on one condition)

Regenerated `tests/shaders/references/*.rslo` (the `ShaderCompilerImmutability` references; `somewood.rslo` is the `SloinfoGoldenOutput` fixture) from the new compiler's output. Compared with the previous checked-in files, over **all 108 references, 7617 lines**:

| Kind of difference | Lines |
|---|---|
| `#!version 1.0.0` -> `#!version 1.1.0` | 108 (one per file) |
| a stochastic instruction gained a trailing ordinal operand (and nothing else on the line changed) | 63 |
| **anything else** (other changes, added or removed lines, a stochastic instruction without an ordinal) | **0** |

The ordinals of every file are exactly 0..n-1 in file order. `git diff --stat` for the directory agrees: 108 files, 171 insertions, 171 deletions (108 + 63). `ShaderCompilerImmutability` and `SloinfoGoldenOutput` pass again.

#### Notes

- Keys are meaningless on rays until Phase 5 sets `CRay::key`; nothing reads them under `Legacy`.
- 024 merge: unchanged (`recording_services.h` untouched by this phase).

#### Phase 4 follow-up (maintainer decisions)

- **`Sampling_ObjectIds_splits`**: amended as approved. It now asserts `reports().size() > 1` (the patch was split into several grids), that every grid reports the same id, and that this id is `mixChild(patch[0]->stableId, 0)`, the mesh's child. Only those assertions changed. Passes.
- **`LibShader_StaleShaderRejectedJit`**: test unchanged; the code was fixed. `CLLVMJitEngine::compileShader` (`src/libshader/shading/llvmJit.cpp`) now reports a failed add/bind through the diagnostic sink with `error(CODE_BADFILE, ...)`, naming the shader and file, saying the `.slo` is stale or incompatible and must be recompiled with `oshader --jit`, and including LLVM's text. The unresolved-symbol list is not part of the lookup error (that says only `Failed to materialize symbols`); it arrives through the ORC session's error reporter, so the engine now installs one that still prints `JIT session error: ...` to stderr and keeps the text for the diagnostic. Captured diagnostic: `Cannot bind JIT shader "stale_probe" (...stale_probe.slo): the .slo is stale or incompatible with this renderer and must be recompiled with `oshader --jit`. Failed to materialize symbols: { (main, { _stale_probe }) } (Symbols not found: [ _op_random_f ])`. Passes.
- Full suite: **651/651 pass**; `ctest -L architecture` 21/21.
- The `.rslo` 2.0.0 / `.slo` semver change is on hold, not started; no references regenerated for it.

### Phase 4 follow-up: format versioning, tests first (T056a)

New tests `LibShader_Format_*` (`src/libshader/tests/test_format_versions.cpp`, nine ctest entries) written against the unchanged loaders. Against the current code: 2 pass (`rslo_current_loads`, `slo_current_loads`, the controls) and 7 fail for the intended reasons:

| Test | Why it fails now |
|---|---|
| `rslo_old_refused`, `rslo_newer_refused` | the file is refused (exact major+minor match) but the message is `Error in shader ... ("Version mismatch") (v1.0.0, .rslo format 1.1)`: it names neither the file's version nor the supported one and does not say to recompile |
| `rslo_lower_minor` | a same-major, lower-minor file (1.0 under 1.1) is refused: the check is equality. (After the bump to 2.0 no lower minor exists; the test then reports SKIPPED through `SKIP_RETURN_CODE 77`, it does not pass vacuously) |
| `slo_emitted` | `oshader --jit` writes no `openrender.slo.format` node |
| `slo_old_refused`, `slo_missing_refused`, `slo_newer_refused` | the module is a valid, linkable one whose format node says 1.0.7 / is absent / says a newer version, and it loads: there is no format check |

Fixture versions use patch 7 so a string cannot match by accident (the loader's message already contains the renderer version `v1.0.0`). `test_stale_shaders.cpp` (the approved link-failure backstop) is untouched. `src/common/sloFormat.h` (2.0.0) exists only so the tests compile.

### Phase 4 follow-up: format versioning, implementation (T056c, T056d)

- **Rule** in `src/common/formatVersion.h` (`formatVersionReadable()`: `file.major == supported.major && file.minor <= supported.minor`, patch never checked; `parseFormatVersion()`, a strict `M.m.p` parser), documented in `rsloFormat.h`, `sloFormat.h`, `contracts/shading-runtime.md` (V1-V4, I1, J1) and `research.md` (R3.6).
- **`.rslo`**: `RSLO_FORMAT` 2.0.0; both lexers (`runtime/rslo.l`, `shading/rslo.l`) use the rule instead of major+minor equality; the refusal reads `Version mismatch: this file is .rslo format 1.0.7, this renderer reads format 2.0.0 (same major, minor up to 0); recompile the shader with oshader`.
- **`.slo`**: `SLO_FORMAT` 2.0.0 (separate from the `.rslo` one); `oshader --jit` writes `openrender.slo.format` (three i32 operands); `CLLVMJitEngine::compileShader` reads it (`readFormat`, missing or unreadable = 1.0.0) and checks it before adding or linking the module, refusing with `error(CODE_BADFILE, ...)` that names shader, file, the module's format, the supported format and `oshader --jit`. `openrender.shader.version` is unchanged; its `std::stoi` is replaced by `parseFormatVersion` (the field keeps the major number; nothing else reads it). The `_v2` renaming and the link-failure diagnostic are the backstop (`LibShader_StaleShaderRejectedJit` passes unchanged).
- **References**: the 108 `tests/shaders/references/*.rslo` regenerated. Against 659c770, over 108 files / 7617 lines: 108 lines changed, every one `#!version 1.1.0` to `#!version 2.0.0`; **any other difference: 0**. `git diff --stat`: 108 files, 108 insertions, 108 deletions. `ShaderCompilerImmutability` and `SloinfoGoldenOutput` fail against the old references (as they must) and pass against the regenerated ones.
- **Tests**: full suite **660 tests: 659 passed, 1 skipped, 0 failed** (the 651 plus the nine `LibShader_Format_*`); the skip is `LibShader_Format_rslo_lower_minor` (supported minor is 0, so no lower minor exists; reported SKIPPED, not passed). `ctest -L architecture` 21/21. No stale shader (0 of 216 compiled shaders older than `oshader`).

## Phase 5 (US3): correlated camera samples

### Tests first (T057-T058), against the unchanged hiders

New: `Sampling_CameraSlotsEqual` (unit, label `sampling`), `CameraSamples_JitterZeroUnchanged_{reyes,raytrace}` and `CameraSamples_SobolEngaged_{reyes,raytrace}` (visual, labels `visual;regression;sampling`; driver `test_render_compare`, scenes `examples/rib/tests/sampling-camera-jitter{0,1}-*.rib`). Full suite after adding them: 665 tests, 660 pass, 1 skipped (`LibShader_Format_rslo_lower_minor`, as before), **3 fail**, all new; `ctest -L architecture` 21/21. The temporary probe implementation used to check the unit test is satisfiable has been reverted (the seam bodies are stubs).

| Test | Result now | Why |
|---|---|---|
| `Sampling_CameraSlotsEqual` | FAIL | the seam is a stub: `reyesAbsSub`/`raytraceAbsSub` return 0 (unit-conversion checks fail) and `cameraSample` returns zeros (the slot-defined values and camera key are not produced). Verified satisfiable: a temporary 25-line implementation passed it, then was reverted. |
| `CameraSamples_SobolEngaged_reyes`, `_raytrace` | FAIL | `0 of 76800 value(s) differ`: with jitter 1 the sobol and legacy renders are identical, because the hiders ignore the sampler for camera samples |
| `CameraSamples_JitterZeroUnchanged_reyes`, `_raytrace` | PASS | **a guard, not a red test**: with jitter 0 the sobol render equals the legacy one today because the option is ignored. It becomes meaningful once the hiders use the slots; it is the test T061-T063 must keep green. The engaged control above is what stops it passing for the wrong reason. |

### Implementation (T060-T064)

- **`src/ri/hiders/sampler.{h,cpp}`**: `CSampler::reyesAbsSub`, `raytraceAbsSub` and `cameraSample()` (the shared absolute-subpixel function: `SampleId` from floor division/modulo, `jitter*(u-0.5)+0.5001011` with `u` from the Film2D/Time1D/Importance1D slots, time stratum `row*nx+column`, transposed importance stratum `column*ny+row`, camera key, and the lens as the Shirley-Chiu concentric map of the Lens2D slot, 0 without depth of field). `Legacy` code is untouched.
- **`stochastic.cpp`**: under a non-Legacy backend each sample is `cameraSample(cfg, reyesAbsSub(left, j), reyesAbsSub(top, i), wantLens)`; `apertureGenerator` and the Option B table are not used on that path.
- **`raytracer.cpp`**: `sample()` computes `raytraceAbsSub(left, nx, xSampleOffset, i+x)` (and y), calls the same function, and puts the importance slot in `cRay->jimp`, the camera key in `cRay->key`, depth 0 and `cameraImportance = true`; `computeSamples()` reads the lens from the ray under the new backends.
- **`trace.cpp`**: `trace(CRay*)` keeps `jimp` for a ray with `cameraImportance` (also across transparency continuations); every other ray still draws `urand()` until Phase 6.
- **`ray.h`**: `CRay::key` and `depth` get default initialisers, plus `bool cameraImportance = false`.
- Decision recorded: research R3.1 ("Lens mapping") and `contracts/sampler-api.md`.

### Test amendment (the one approved change to an approved test)

`tests/unit/sampling/test_camera_slots.cpp`: the "first attempt of the rejection loop" lens check is replaced by a comparison with the Shirley-Chiu concentric mapping of the lens slot (computed in the test, double precision, tolerance 5e-6), keeping the strictly-inside-the-disk check. Against the stubs it failed at that line for every lens sample (36936 failures of the new check, among the others). Hunks (`git diff HEAD -- tests/unit/sampling/test_camera_slots.cpp`): the header comment item 3, the new `concentricDisk()` helper, the lens block in the loop, and the removal of the `acceptedFirst`/`rejectedFirst` counters and their print. Nothing else in the file changed.

### Results (T064)

- `Sampling_CameraSlotsEqual`, `CameraSamples_SobolEngaged_{reyes,raytrace}`: green. `CameraSamples_JitterZeroUnchanged_{reyes,raytrace}`: stay green (`0 of 76800` and `0 of 19200` values differ).
- Full suite: **665 tests, 664 passed, 1 skipped** (`LibShader_Format_rslo_lower_minor`); `ctest -L architecture` 21/21; `ctest -L shading_parity` 11/11. Everything runs under `Legacy` by default, so the visual suite is unchanged.
- **Parity sweep with `sobol` (by hand, no threshold change).** Each parity test's two scenes were copied to a scratch directory with `Option "sampling" "sampler" ["legacy"]` and again with `["sobol"]` after the `version` line (T079's `--prepend` does not exist yet), run through the same `test_hider_parity` command and environment. 109 tests give a block-average figure (the five depth parity tests, `Parity_depth*`, print none; they exit 0 under both samplers). Results: **no test exceeds its threshold under either sampler**; the mean change is -0.86; 13 tests are more than 0.5 higher under sobol, 52 more than 0.5 lower. The largest increases: `Parity_blobby-tolerance-tight-reyes-zbuffer` 2.52 to 4.91 (threshold 20); `Parity_csg-sphere-cube-intersection-raytrace-reyes` 2.12 to 4.45 (threshold 20); `Parity_blobby-tolerance-inherited-reyes-zbuffer` 3.02 to 5.19 (threshold 20); `Parity_blobby-csg-difference-reyes-raytrace` 3.73 to 5.70 (threshold 20); `Parity_blobby-hand-maxed-reyes-zbuffer` 3.36 to 4.30 (threshold 20). The depth-of-field pair improves (`Parity_dof` 51.97 to 40.47 against 60, `Parity_dof-motion` 26.33 to 22.66 against 40). The closest to its threshold is `Parity_transparency-matte-aov`, 23.83 against 25 (23.86 under legacy; a known flaky test). Legacy figures differ from a plain `ctest` run by thread-timing noise (for example `Parity_dof` 47.94 there). The full table is the scratchpad `parity_sweep.json`; no reference or threshold was touched.

## Phase 6 (US1/US6): transport and other consumers

### Tests first (T065-T068), against the unchanged shading code

Total tests: 698 (665 + 33 new). New: 21 fail (the red ones), 12 pass (guards and controls, below). The only other failure in the full run was `Parity_transparency-matte-aov` (known flaky; 23.88 against 25 when re-run alone, passes). `ctest -L architecture` 21/21. New shaders: `sampling_random_probe`, `sampling_jit_probe`, `sampling_independence_probe` (three new reference `.rslo` files in `tests/shaders/references/`, new files only; `ShaderCompilerImmutability` and `SloinfoGoldenOutput` pass). The driver `tests/visual/test_render_compare.cpp` gained `--a:<opt>`, `--b:<opt>`, `--channels=` and `--threshold=` (existing calls unchanged).

**An observation that shapes the red/green split.** Under `Legacy` the shading draws come from a per-context Mersenne stream that the worker threads share out between them, so even a `-t:1` render of a random()-based scene varies run to run: the same scene rendered twice differs in about 50 of 2304 pixels. Two consequences: every `same` test on a draw-dependent value is red today, partly for that reason, and a `differ` test against any other render would pass today for the wrong reason, so the controls compare two seeds (today the seed does not reach the shading draws) and a `SamplingJitDeterminism_*` test pins the run-to-run noise itself.

| Test(s) | Today | Why |
|---|---|---|
| `SamplingJitParity_RandomProbe_{rslo,slo}` | FAIL | R = 1, want 0: `Du()` of a varying `random()` is not 0 (the derivative-offset vertices draw their own value). G (four distinct draws) and B (urandom distinct, all in [0,1)) already hold. |
| `SamplingJitParity_<fn>` for random_loop, random_urandom, random_vector, trace_transmission, occlusion, indirectdiffuse, gather (7) | FAIL | rslo and slo render the probe sphere differently (about 130-180 of 9216 values): each backend consumes the context stream in its own order, and the stream itself varies run to run |
| `SamplingJitDeterminism_<fn>` for the same 7 | FAIL | the same scene rendered twice differs (the shared stream) |
| `Sampling_ShaderDrawIndependence_second_loop`, `_illuminance` (T066) | FAIL | 62 of 2304 values differ when the first trace loop runs one more iteration: the later call sites' draws shift |
| `Sampling_ThreadSweep_{quadlight,sphere-random,sphere-gather}` (T068) | FAIL | `-t:1` against `-t:4` differs (74049 of 307200, 23214 and 23358 of 1228800 values) |
| `SamplingJitParity_visibility`, `SamplingJitDeterminism_visibility` | PASS (guards) | `visibility(P, Q)` in its plain form does not depend on the draws in this scene (it also showed no change when the bucket order changed) |
| `Sampling_ThreadSweep_sphere-trace` | PASS (guard) | `trace_probe`'s plain `trace(P, V)` does not depend on the draws either |
| `Sampling_ShaderDrawIndependence_first_loop_changes` | PASS (control) | the first loop's channel does change with n1, so the variant is real |
| `SamplingJitEngaged_<fn>` for the 7 draw-dependent functions | PASS (control) | seed 0 against seed 1 differs today only through the stream noise above; after the conversion it must differ because the seed reaches the draws, and it cannot pass for the wrong reason once `SamplingJitDeterminism_*` is green |
| `Sampling_ProbeIndependence` | PASS (guard) | the midpoint probe's `trace()` no longer draws `jimp` for a primary ray under sobol (the Phase 5 `cameraImportance` flag), so mid and min already agree bit for bit |

T067: `tests/visual/check_no_global_randomness.sh`, not registered. Run by hand it reports **95 matches** outside comments (exit 1): `pointHierarchy.cpp` 16, `common/random.h` 8, `hcshader.cpp` 7, `photon.cpp` 6, `executeMisc.cpp` 6, `geometryDispatch.cpp` 5, `radiance.cpp` 5, `irradianceDispatch.cpp` 5, `shading.h` 5, `raytracer.cpp` 4, `object.cpp` 4, `scriptFunctions.h` 4, `rslOps.cpp` 4, and a few more files with one to three each (`photon.h`, `shading.cpp`, `common/random.cpp`, `init.cpp`, `trace.cpp`, `stochastic.{h,cpp}`, `sampler.cpp`, `radiance.h`, `display/file/file_base.cpp`).

### T067 follow-up: the scan has an allowlist and is registered

`Sampling_NoGlobalRandomness` (label `sampling`) runs `tests/visual/check_no_global_randomness.sh` with `tests/visual/no_global_randomness_allowlist.txt`. It fails on a match in a file not on the allowlist and on a stale allowlist line. **Red today: 67 non-allowlisted matches in 13 files**, exactly the sites Phase 6 converts: `shading.cpp` (3, gather, T071), `executeMisc.cpp` (6) and `scriptFunctions.h` (4) and `rslOps.cpp` (4) (T071, T072), `trace.cpp` (1, secondary-ray `jimp`), `hcshader.cpp` (7, T074), `photon.cpp` (6, T075), `irradianceDispatch.cpp` (5) and `radiance.cpp` (5) (T076), `geometryDispatch.cpp` (5), `object.cpp` (4), `pointHierarchy.cpp` (16) and `init.cpp` (1) (T077). The allowlist holds ten files, all legacy generators or the Legacy camera path that Phase 9 deletes: `common/random.{h,cpp}` (T092), `shading.h` (T091), `stochastic.h` and `photon.h` and `radiance.h` (T092), `stochastic.cpp` and `raytracer.cpp` (T090/T091), `sampler.cpp` (T093), `display/file/file_base.cpp` (T095). Checked by hand: allowlisting every matching file makes the scan pass (exit 0), and a line for a file with no match fails it as stale (exit 1).

### T067 follow-up 2: the scan also matches `random2d/3d/4d`

The pattern now includes `\brandom[234]d\b` (quickstart §9, the script and the tasks text agree). The definitions stay under the existing `shading.h` allowlist line (reason updated: its uses elsewhere are not allowlisted); no entry was added. `Sampling_NoGlobalRandomness` is red with **79 non-allowlisted matches in 14 files** (was 67 in 13): the 12 new hits are `texture.cpp` (6, T073, a new file), `irradianceDispatch.cpp` (+2, T076), `shading.cpp` (+2, T071) and `executeMisc.cpp` (+2, T071). Per file: `pointHierarchy.cpp` 16 (T077), `executeMisc.cpp` 8 (T071), `irradianceDispatch.cpp` 7 (T076), `hcshader.cpp` 7 (T074), `texture.cpp` 6 (T073), `photon.cpp` 6 (T075), `geometryDispatch.cpp` 5 (T077), `radiance.cpp` 5 (T076), `shading.cpp` 5 (T071), `object.cpp` 4 (T077), `scriptFunctions.h` 4 (T072), `rslOps.cpp` 4 (T072), `init.cpp` 1 (T077), `trace.cpp` 1 (T071). Re-checked by hand: allowlisting every matching file gives exit 0, and an extra line for a file with no match gives exit 1 (stale).

### Implementation (T070-T078)

- **T070** `common/random.{h,cpp}`: `sampleHemisphere`, `sampleCosineHemisphere` (Sample2D) and `sampleSphere` (Sample3D) overloads; no rejection loop, same distributions. The `CSobol&` overloads stay until T092.
- **T071/T072** `shading.h`: the draw members (contract D1-D3). `random()`/`urandom()` (interpreter macros in `scriptFunctions.h`, JIT `op_random_{f,v}_v2`), the ray sample draws of `traceTransmission`/`traceReflection` (`sampleRay`, cone, time), `gatherHeaderRay`/`gatherSample`, `jitTraceBatch`, `jitOcclusionBatch`, the macros of transmission/visibility/trace, occlusion/indirectdiffuse, gather, texture, environment and shadow (`DRAW_BEGIN`/`DRAW_STEP`/`DRAW_BIND`), and `trace(CRay*)` for rays with no camera importance (child keys on spawned rays).
- **T073** the six lookups of `texture.cpp` read the draw cursor (deviation from the `SubstreamKey` parameter, contract T1).
- **T074** `hcshader.cpp`: the hardcoded sphere and quad lights draw per shading point (`bindDraws`, pseudo call sites 0xFF01, 0xFF02).
- **T075** photons: `CJob::firstPhoton`, `cellKey(light, index)` per photon, `resetDrawOccurrences()` per chunk (see tasks.md).
- **T076** `irradianceDispatch.cpp` (cursor, bound by the occlusion/indirectdiffuse macros and `jitOcclusionBatch`) and `radiance.cpp` (keyed streams of the cluster).
- **T077** `object.cpp` `getDisp` (constant key), `geometryDispatch.cpp` `cluster()` (object, iteration) and `CPoints::dice` (object, size, iteration), `pointHierarchy.cpp` (node), `init.cpp` (shader, call site, draw number). These use the keyed streams whatever the sampler.
- **Deviation, uniform draws**: the invocation-level (`batchKey`) branch of `drawSubstream` is implemented but unreachable from RSL source (contract I2).
- **Deviation, shader key**: the shader key is a hash of the shader name only, with no instance ordinal; two instances of one shader at one point share a stream.
- **Deviation, test fixture**: the Phase 6 tests that share `build/tests/visual/sampling-phase6` and write the same TIFF names hold the ctest resource lock `sampling_phase6_scratch`; before it a `-j4` run could race (`SamplingJitDeterminism_gather` failed once for that reason, passes in isolation). Assertions are untouched.

### Results (T078)

- Full suite: **699 tests: 695 pass, 1 skipped (`LibShader_Format_rslo_lower_minor`), 3 fail**. `ctest -L architecture` 21/21, `ctest -L shading_parity` 11/11. Every existing visual, parity and libshader test passes under the default Legacy sampler; no reference was regenerated and no threshold touched. Stat check: 0 of 216 compiled shaders older than `oshader`.
- Green now: all 24 `SamplingJit{Parity,Determinism,Engaged}_*`, `Sampling_ShaderDrawIndependence_illuminance` and `_first_loop_changes`, `Sampling_ThreadSweep_*` (all four, at `-t:1` against `-t:4`), `Sampling_ProbeIndependence` and `Sampling_NoGlobalRandomness`. The allowlist is unchanged (ten files, none stale).
- A smoke render of the probe under `pmj02` and `independent` (random loop, trace/transmission, occlusion, gather) runs and gives plausible values.

### Two approved tests cannot pass as written (not edited)

1. **`SamplingJitParity_RandomProbe_{rslo,slo}`** (R channel, `Du()` of a draw must be 0). The scene uses the reyes hider, whose grids take `Du()` from neighbouring grid vertices; independent keyed draws at neighbouring vertices differ, so `Du(random())` is not 0 on a reyes grid by construction. It is 0 only for ray hits, where the derivative-offset vertices mirror their real vertex (R3.10a). Evidence: the same scene with `Hider "raytrace"` reads **(0, 1, 1)** in both formats, i.e. R 0, G 1, B 1. Proposed fix: `Hider "raytrace"` in `examples/rib/tests/sampling-random-probe.rib.in`.
2. **`Sampling_ShaderDrawIndependence_second_loop`**. The probe's `trace(P, dir)` rays hit the probe sphere itself, whose shader runs again at depth 1 and returns a Ci whose R channel is the first loop's value, so the second loop legitimately depends on `n1`. Evidence: with the second loop reduced to `random()` the channel is bit-identical for n1 = 2 and 3; with a distance trace (`float c = trace(P, dir)`, nothing shaded) also identical; with the colour trace 62 of 2304 values differ. Proposed fix: the float (distance) form of `trace()` in `shaders/sampling_independence_probe.sl` (and its reference `.rslo`).

### Phase 6 follow-ups (maintainer decisions)

1. **RandomProbe.** Contract J4 and research R3.10a now say `Du/Dv(random()) == 0` holds for ray-hit shading only. `sampling-random-probe.rib.in` takes the hider as a parameter: `SamplingJitParity_RandomProbe_{rslo,slo}` use `raytrace` and check all three channels (green), and the new `SamplingJitParity_RandomProbeReyes_{rslo,slo}` use `reyes` and check G and B only (`test_pixel_probe --channels=1,2`, a new option; existing calls are unchanged). The reyes variant was written and shown green first (while the three-channel check was still on reyes and red), then the three-channel check was moved to raytrace.
2. **second_loop.** `sampling_independence_probe.sl` returns the constant (0.25, 0.25, 0.25) at ray depth > 0 (`rayinfo("depth", ...)`, selected arithmetically). Only its reference `.rslo` was regenerated; the diff against the committed one is the added code only: three new variables (`rayDepth`, `depthFound`, `nested`), the `rayinfo` and `step` instructions, and the blend of the final Ci (`(1 - nested) * color(...) + nested * constant`), with the compiler's temporaries renumbered by it (`temporary_11` changes type, `temporary_12` to `temporary_15` are new); no other instruction changed. `Sampling_ShaderDrawIndependence_second_loop` is green.
3. **Shader key** (answer in the report): nothing in the key distinguishes two instances of a shader. New test `Sampling_InstanceIndependence_{after,before}` (shaders `sampling_instance_light.sl` and `sampling_instance_probe.sl`, scenes `sampling-instance-{alone,after,before}.rib`): the light with id 2 must draw the same at a point whether or not another instance of the same shader is declared before or after it. Today `_after` fails (32 of 1024 values) and `_before` passes (the lights execute in declaration order and share one occurrence counter per call site, so the second light to run gets occurrence 1). Fix awaits approval of the test.
4. The ctest resource lock `sampling_phase6_scratch` is accepted.

Full suite after these changes: 703 tests, 700 pass, 1 skipped, 2 fail: `Sampling_InstanceIndependence_after` (the new red test) and `Parity_transparency-matte-aov` (the known flaky test; passes when re-run alone). `ctest -L architecture` 21/21; the randomness scan is green with the allowlist unchanged.

### Phase 6 follow-up: the displacement-only passes (tests first)

`shade()` gives no point a sample identity in a displacement-only pass (the dicer's bounding and grid-size probes), so a displacement shader's `random()` there draws from the stale `pointKeys` and occurrence counters of the thread's state; the full shading pass then displaces with proper keys. Tests, red today (full suite then: 706 tests, 702 pass, 1 skipped, 3 fail in the run: `Sampling_InstanceIndependence_after`, `Sampling_DisplaceThreadSweep`, `Sampling_DisplacePassConsistency_sphere`; `ctest -L architecture` 21/21):

| Test | Today | Meaning |
|---|---|---|
| `Sampling_DisplaceThreadSweep` (`sampling-displace-sphere.rib`, `-t:1` against `-t:4`) | FAIL, 689 of 76800 values differ | a `random()`-displaced sphere depends on the thread count |
| `Sampling_DisplaceRepeat` (`-t:4` against `-t:4`) | FAIL in 3 of 6 runs (689, 1166 and 480 values differ; 0 in the others) | and varies between two runs, as thread timing decides; it can only fail for the stale-key reason, never pass wrongly once the fix lands |
| `Sampling_DisplacePassConsistency_sphere` (`tests/unit/sampling/test_displace_consistency.cpp`) | FAIL | 16 displacement-only and 10 full reports; 1407 points seen by both kinds of pass, all 1407 displaced differently; 239 points displaced differently within the probe passes; 0 within the full passes |

The consistency test uses a second test hook, `CShadingContext::pointDisplacedObserver` (declared next to `pointKeyObserver`, null in production), called right after a displacement shader with the real vertices' `(u, v)` and displaced `P`. It pairs the points that both kinds of pass see (same object, same `(u, v)` bit for bit) and requires bit-identical `P`. Premise check: with a displacement that does not draw (`P += 0.05 * N`) the same test sees 1134 shared points and 0 mismatches, so a mismatch with `random()` can only come from the draws. The hook is the only production change in this step; Legacy is untouched (the new tests run under sobol).

### Phase 6 follow-up fixes (instance key, displacement-only passes)

- **Instance key.** `CShaderInstance::instanceKey` (set by `CRendererContext::getShader`: the light's RIB handle, else a creation ordinal) is mixed into the shader key of `drawCounters()`, which also keys the occurrence counters per instance. The RIB parser appends `"__handleid"` / `"__handlename"` to a light's parameters; `RiLightSourceV`/`RiAreaLightSourceV` strip them before the shader sees them and `CRibOut` does not write them. `Sampling_InstanceIndependence_after` and `_before` are green.
- **Displacement-only passes.** `shade()` now fills the point keys in every call of a non-Legacy frame (`wantKeys` no longer excludes `displaceOnly`); the displacement-only path also requests `PARAMETER_U | PARAMETER_V` so (u, v) are in the surface's parametric space as in the full pass; `pointKeyObserver` is still called for full passes only (`Sampling_CellKeyUniqueness_*` count shaded grids). `Sampling_DisplaceThreadSweep` and `Sampling_DisplacePassConsistency_sphere` are green; `Sampling_DisplaceRepeat` passed 20 of 20 runs (it failed 3 of 6 before); `Sampling_DisplaceThreadSweep` passed 20 of 20.
- **Legacy.** `wantKeys` is false, `drawCounters()` returns null and no draw changes under Legacy; the only things Legacy now does differently are cosmetic: an extra pointer member on shader instances and the parser's handle parameter, taken out again before any shader sees it.
- **Results.** Full suite 706 tests: 705 pass, 1 skipped (`LibShader_Format_rslo_lower_minor`), and in this run `Visual_camera-dof-reyes` failed (a Legacy camera-lens test the changes do not touch: 12 further runs alone gave 15.7 to 18.4 against a threshold of 20, one run in 6 earlier failed, as it did once in a previous full run); `ctest -L architecture` 21/21; `-L shading_parity` 11/11; the randomness scan is green with the allowlist unchanged.

### The light-handle parameter and the parser buffers

A reported overflow: the two LightSource rules of `rib.y` append `"__handleid"` / `"__handlename"` to the parameter list without `paramCheck()` first.

- **Analysis: no overflow is possible.** The parser buffers start at 20 entries. `paramCheck()` runs right after every parameter is counted and grows them (by 10) the moment `numParameters == maxParameter`, so after a parameter list `numParameters < maxParameter` always holds; the `ribPL` rule asserts exactly that. `tokens[numParameters]`, where the handle parameter goes, is therefore always a free slot.
- **Confirmed with a Guard Malloc test, written and not kept.** A test that parsed lights with 19, 20, 29 and 30 parameters in both handle forms, under macOS Guard Malloc (every allocation at the end of its own page, so a write past a 160- or 240-byte buffer crashes at once), passed on the code as it was: the parameter lists and instance parameters arrived intact and the instance keys came from the handle. With the growth condition deliberately broken (`numParameters == maxParameter + 1`) the same test crashed at the 20-parameter case, so it could have caught the overflow. It is not part of the branch (the maintainer left it out); its files are left uncommitted in the worktree.
- **Kept: the defensive `paramCheck()`.** `rib.y` now calls `paramCheck()` before each of the four appends (numeric and string handle, `LightSource` and `AreaLightSource`), with a comment stating the invariant. It never grows anything today.

## Phase 7 (US1): determinism harness

### Tests first (T078a), against the unchanged driver

`DeterminismHarness_*` (8 tests, `tests/visual/check_determinism_harness.sh`) exercise `test_depth_determinism`'s new cross-configuration mode through the interface of `contracts/test-harnesses.md`: `--configs "<opts>|..."`, `--outputs a.tif,b.tif`, `--prepend "<RIB line>"`, `--keep-pins`, `--no-dither`, plus one flag the contract does not name, `--print-scene` (print the prepared copy of the scene and exit 0 without rendering). Against the current driver 7 fail (it prints its usage and exits 1: the flags do not exist) and `usage_unchanged` passes (a guard: the original usage is untouched). The premises of the cases were checked with the existing comparison driver: `determinism-harness-noisy.rib` (Legacy `random()` shading, no pin) differs between `-t:1` and `-t:4` (20196 of 307200 values); `determinism-harness-two-outputs.rib` under sobol is identical across thread counts for both its colour and depth outputs; and 8-bit `sphere-random-reyes` under sobol with `Quantize "rgba" 255 0 255 0` (no dither) is byte-identical across `-t:1`, the default and `-t:4`.

Full suite with these added: 714 tests, 8 new (7 red, 1 green guard); the other 706 pass (705, plus the skipped LibShader_Format_rslo_lower_minor); `ctest -L architecture` 21/21.

### Implementation (T079-T083)

- **Driver** (`tests/visual/test_depth_determinism.cpp`): `--configs`, `--outputs`, `--prepend`, `--keep-pins`, `--no-dither` and `--print-scene`, as approved. The original usage is untouched (`DeterminismHarness_usage_unchanged`, `DepthFilter_*`, `ColorDeterminism_*` pass). Outputs are decoded by libtiff at any depth (`readRawTiff` in `depth_tiff.h`: 8/16/32/64-bit, interleaved or separate planes, kept as the decoded bytes and compared bit for bit); a difference prints at most 5 `pixel (x, y) channel c = ..., <first> had ...` lines per output. The prepared copy of the scene is written to the working directory under the name `determinism-<scene>.rib` (so relative archive paths resolve as for the original), removed on success and kept on a difference. The line-editing code is shared with `test_render_compare` in `tests/visual/scene_copy.h`. A line that mentions `numthreads` is dropped whole, comments included (the approved case `print_prepend` requires that no `numthreads` text is left).
- **Registration** (`tests/visual/CMakeLists.txt`): `add_determinism_sweep(<name> <rib> <outputs...> [CONFIGS <list>] [COPY <dir>...])` registers `DeterminismSweep_<name>` with the default configurations `-t:1||-t:2|-t:4|-P:2`, `--no-dither`, and the labels `visual;regression;determinism;determinism-sweep`. The prepended line is `Option "sampling" "sampler" ["sobol"]` (the spelling the approved harness cases use; tasks.md's `"string sampler"` form is the same option with an inline declaration).
- **Scene list** (`tests/visual/determinism-scenes.cmake`, T081): 413 scenes, every output of each. They are the scenes under `examples/rib` that a visual, parity or csg test renders (the ctest command lines, plus the scenes the csg metric tests name inside their config files). Not swept, as comments in the file: 18 GI scenes (shaders that call `occlusion()`/`indirectdiffuse()`/`photonmap()`, and `Hider "photon"` scenes), the 2 `*-legacy.rib` controls (they select the Legacy sampler themselves), the 3 `csg-malformed-*` scenes and `csg-procedural-missing-archive` (expected to fail), and `teapot.rib` (no Display). No scene sets its own non-zero-dither `Quantize`.
- **T082**: `DepthFilter_DeterminismThreads_{avg,mid}` and `ColorDeterminism_Threads_{transparent,opaque}`, `-t:1|-t:2|-t:4`, no prepended option (the scenes are jitter 0 and unpinned). The existing registrations are unchanged.
- **T083**: `CameraSamples_JitterZeroUnchanged_*` now render the original scene against the same scene with the sobol line prepended (`test_render_compare` gained `--prepend-a:`/`--prepend-b:`); `Sampling_ThreadSweep_*` run `test_depth_determinism --configs "-t:1|-t:4"` on the original scenes (`quadlight.rib`, `sphere-{random,trace,gather}-reyes.rib`) with a two-line prepend, the sobol line and `Quantize "rgba" 0 0 0 0`, so they still compare 32-bit float output as the hand-written copies did. The 6 hand-written copies (`sampling-threadsweep-*.rib`, `sampling-camera-jitter0-*-sobol.rib`) are deleted. The converted thread sweeps are not vacuous: with the sobol line replaced by `Option "sampling" "sampler" ["legacy"]` the same driver reports 24122, 4940 and 99222 differing values for `sphere-random`, `sphere-trace` and `sphere-gather` (`-t:1` against `-t:4`).

### Results (T084): the sweep is not clean

- **Timing (M3)**: `ctest -L determinism-sweep -j4` runs 413 tests in **421.9 s (7.0 min)**, under the 10-minute limit of FR-030, so the whole sweep kept the `determinism` label at that point. (Superseded: later sweeps took 10 to 12 minutes; see T084d for the split.) The full suite (1131 tests, `-j4`) takes 476.5 s.
- **Result**: first run (the sweep alone) 394 pass, **19 fail**; second run (inside the full suite) **18 fail**. The sets differ: 24 distinct scenes failed at least once, 13 failed in both. Stop and report: no threshold was touched and no scene was removed from the list.
- **Classification**, from repeating the failing scenes under controlled conditions (`-t:1` x4, `-t:2` x3, `-t:4` x5 or x6; each run compared with the first; 8-bit undithered output unless said):

| Class | Scenes | Evidence |
|---|---|---|
| A. Thread race that does not involve the sampler | `sphere-shader-parameter-reyes-slo`, `sphere-shader-parameter-reyes` | Deterministic shader (no `random()`), `-t:1` x4 identical, `-t:4` x5 differ (694 to 1057 values), still 694 values with `jitter 0` and the Legacy sampler; in float output 0.776 against 0.750, a shader value, not a rounding step. The `.rslo` twin failed in the second run (`-t:2` against `-t:1`). Cause not found. |
| B. Differs from run to run at **one** thread, or with `jitter 0` | `csg-hero-translucent-raytrace`, `csg-hero-two-shaders-raytrace`, `csg-secondary-rays-reyes`, `csg-hero-zbuffer-1600`, `csg-hero-stats-1600-reyes`, `csg-procedural-operand-raytrace`, `csg-procedural-operand-inline-raytrace` | Under Legacy, `-t:1` x4: translucent 1762 values, two-shaders 2243, secondary-rays-reyes 3558. With Legacy and `jitter 0`: `csg-hero-zbuffer-1600` differs at `-t:1` x4 (13 values) and `-t:4` x5 (1); `csg-hero-stats-1600-reyes` at `-t:4` x5 (3 values), and with sobol and `jitter 0` at `-t:1` x4 (17) and `-t:4` (7); `csg-secondary-rays-reyes` with sobol and `jitter 0` at `-t:1` (3) and `-t:4` (74). The two procedural-operand scenes are clean at Legacy `-t:1` x4 and at sobol with `jitter 0`, but differ at sobol `-t:1` x3 (7 values, the same pixel `(378, 128)` each time). These are not thread-count dependent, so no sampler change fixes them. Cause not found (all are CSG scenes). |
| C. Few values, intermittent, seen only with jitter on | examined: `blobby-dent-reyes`, `blobby-repeller-heights-reyes`, `csg-hero-plain-sphere-raytrace`, `csg-hero-displaced-raytrace`, `csg-tolerance-inert-raytrace`, `csg-hero-raytrace-800`. Not examined, put here for the shape of their differences: `blobby-blend-far-reyes`, `blobby-spiral-reyes`, `bunny-reyes`, `csg-hero-partial-stats-reyes`, `csg-hero-reyes-1600`, `csg-hero-displaced-reyes`, `csg-offscreen-stats-reyes`, `csg-secondary-rays-raytrace`, `csg-hero-zbuffer-800` | 1 to 35 values, mostly one 8-bit level, at isolated pixels. The same scene passes one run and fails the next, at the same thread count (`-t:4` against `-t:4`). For the six examined scenes, `jitter 0` with Legacy was clean over 4 runs at `-t:1` and 5 at `-t:4`, and so was sobol with `jitter 0`. In float output, `blobby-dent-reyes` and `csg-hero-plain-sphere-raytrace` were identical over 6 runs at `-t:4` (both fail the 8-bit sweep about every other run). Cause not found. |

The repetitions behind the table are small (3 to 6 runs per cell) and these failures are intermittent, so a clean cell is weak evidence and the assignment of the unexamined scenes is a guess. No code was changed to chase any of them.

- `Parity_transparency-matte-aov` failed once in the full run (worst block 27.89 against 25). It is on the known intermittent list and not related to this phase.
### Scope of the bit-identical claim (maintainer decision after T084; updated after the T084b and T084c sweeps)

The claim "bit-identical at any thread count" (SC-001, SC-009, FR-030, FR-031) **excludes the ten scenes disabled in the determinism sweep under GitHub #60, #61 and #63. All three defects predate 039 and are independent of the sampler.** The `DeterminismSweep_*` tests of these scenes stay registered with ctest `DISABLED` (`tests/visual/determinism-scenes.cmake`), with no threshold changed, and the claim applies to a scene again when its issue is fixed.

| Issue | Defect | Disabled scenes |
|---|---|---|
| #60 | A thread race in a deterministic shader: the output differs at `-t:4`, also with `jitter 0` and the Legacy sampler (class A above). | `sphere-shader-parameter-reyes`, `sphere-shader-parameter-reyes-slo` |
| #61 | An uninitialised derivative lane in shading makes `transmission()` renders nondeterministic. T084b fixed the ray-hit part (it enabled `csg-hero-translucent-raytrace`, `csg-hero-two-shaders-raytrace`, `csg-secondary-rays-reyes`, `csg-procedural-operand-raytrace` and `csg-procedural-operand-inline-raytrace` again). The grid-shading part is not fixed: the derivative of a grid lane next to an inactive lane (the terminator of a `raydistant` light) reads the unwritten lane. | `csg-hero-zbuffer-1600`, `csg-hero-stats-1600-reyes`, `csg-hero-reyes-1600`, `csg-hero-zbuffer-800`, `csg-offscreen-stats-reyes`, `csg-hero-partial-stats-reyes`, `csg-hero-two-shaders-reyes` |
| #63 | Intermittent differences of about +-1e-3 on black pixels in the first column of a bucket; no tie, a `ralloc` fill does not explain it, cause unknown. | `csg-hero-displaced-reyes` |

`Visual_sphere-shader-parameter-reyes-slo` is unchanged. Group C is not excluded as such: its reyes part is fixed by T084c (cause: GitHub #62), its raytrace part by T084b, and what is left of it is the #61 and #63 scenes above.

### T084a: root cause of group C (diagnosis only, nothing fixed)

Method: temporary instrumentation (per-sample dumps, hashes of the committed buckets, a `ralloc` fill byte, `MallocPreScribble`), all reverted; `git diff -- src` is empty. Runs are float or 8-bit undithered sobol renders at the thread count named, compared with the first run; "x of n" counts runs that differ from run 1.

**Cause 1 (raytrace scenes): uninitialised `ralloc` memory in the `transmission()` target derivative.**

- *Where the difference starts.* In `csg-hero-plain-sphere-raytrace` (`-t:4`), every sample splatted into the bucket is identical between a good and a bad run except 4 neighbouring samples, which are black (`rgba = 0 0 0 1`) in one run and lit in the other. Alpha, position and key are identical.
- *The shadow ray.* The `raydistant` light calls `transmission(Ps, Ps - dir)`. For the one lane that was followed (same shading point P = (1.45087528, -0.089255929, 8.62987328), same sample key `61786eabe36b8fe1`, same sub-stream, same four jitter values), `dPdu`, `dPdv` and `dDdu` are bit-identical in every run, but `dDdv`, the v-derivative of the target, is:
  - `(-8.45, -7.91, -2.63)`, which is `-D`, in 5 of 10 runs (the neighbour lane read as zero);
  - `9.4e22`, `-6.5e28`, `201`, `-4.2e33`, and similar, in the others (garbage).
  - In `traceTransmission()` the direction is built from `L = D + jitter * dDdv - from`. With a garbage `dDdv` it becomes inf or NaN, `dot(dir, dir) > C_EPSILON` fails, no ray is traced, and the transmission stays 0: a black sample. Otherwise the ray points somewhere arbitrary.
- *The derivative lanes.* Raytrace shading appends offset lanes after the real ones for `du`/`dv`. At a silhouette the offset ray can miss the object, so nobody writes that lane's `Ps - dir`, and `dvVector()` differences it against whatever the allocator handed out.
- *Proof.* `ralloc` memory filled with a fixed byte (0, 85 or 255):

| Scene | Threads | No fill | Filled |
|---|---|---|---|
| csg-hero-plain-sphere-raytrace | -t:4 | 12 of 15 | 0 of 15, for every fill byte |
| csg-hero-displaced-raytrace | -t:4 | 6 of 11 | 0 of 11 |
| csg-tolerance-inert-raytrace | -t:4 | 2 of 11 | 0 of 11 |
| csg-hero-translucent-raytrace | -t:1 | 9 of 11 | 0 of 11 |
| csg-hero-two-shaders-raytrace | -t:1 | 3 of 11 | 0 of 11 |
| csg-procedural-operand-inline-raytrace | -t:1 | 2 of 11 | 0 of 11 |
| csg-secondary-rays-reyes | -t:4 | 11 of 11 | 0 of 11 |
| csg-hero-zbuffer-1600 | -t:1 | 2 of 9 | 0 of 9 |
| csg-hero-reyes-1600 | -t:4 | 1 of 11 | 0 of 11 |

  - The fill also changes the image: with 255, `csg-hero-plain-sphere-raytrace` differs from the unfilled render in 2914 pixels. So the output depends on the content of that memory.
  - `csg-procedural-operand-raytrace` did not differ in the unfilled runs, so the fill says nothing about it.
- *Consequence for #61.* Four of the seven scenes disabled under #61 (`csg-hero-translucent-raytrace`, `csg-hero-two-shaders-raytrace`, `csg-secondary-rays-reyes`, `csg-hero-zbuffer-1600`), and `csg-procedural-operand-inline-raytrace`, are this same defect: it is why they differ at one thread. `csg-hero-stats-1600-reyes` was not tested. So #61 and the raytrace part of group C are one bug. It also affects every scene with a `raydistant`/`transmission()` light, whichever the sampler.

**Cause 2 (reyes scenes): equal-depth fragments, first arrival wins.**

- *Hypothesis tested.* "Fragments at equal depth, composited in thread or bucket order" is confirmed, in a narrower form than #48: it is the opaque test `if (z < pixel->z)` in `drawPixel()` (`stochasticQuad.h`), not the compositing.
- *Where the difference starts.* In `blobby-dent-reyes` the committed bucket differs in exactly one 32x32 bucket, in exactly one sample, `(236.3516, 327.3817)`: alpha 1 and z = 31.6432686 in every run, colour 0.3397 or 0.105 (the ambient term alone). The pre-composite fragment list of that sample holds no competing fragment (`last` alone).
- *The tie.* Instrumenting the opaque test shows that sample receives two opaque fragments with *exactly equal* z. The first one to arrive had colour 0.3397 in 9 of 12 runs, and 0.105 in the 3 runs whose image differs, matching run by run. The scene has 3 such ties per run, the same 3 each time.
- *Not memory.* Every grid shaded in both a good and a bad run has identical `P`, `N`, `Ng`, `Ci` and `Oi` hashes; only the *set* of shaded grids differs slightly (hidden-grid culling), as it does between two good runs. The `ralloc` fill and `MallocPreScribble` do not change the result (`blobby-dent` 8 to 10 of 15 differ in every variant).
- *More scenes.* The runs that differ each contain a tie whose winner differs from run 1 in: `blobby-repeller-heights-reyes` (4 of 4 differing runs), `blobby-spiral-reyes` (2 of 2, with 118 ties per run), `bunny-reyes` (6 of 6).
- *Not explained.* Why the two fragments have exactly equal z (a sample on a shared edge or vertex between two grids, or coincident surfaces) is not established.

**Not explained**

- `csg-hero-displaced-reyes`: differs with and without the fill (2 to 5 of 11) and shows no tie, so it has some third cause.
- `blobby-blend-far-reyes`, `csg-offscreen-stats-reyes`, `csg-hero-partial-stats-reyes`, `csg-hero-stats-1600-reyes`: did not differ in 10 to 14 repetitions of this phase, so there was nothing to attribute.
- Class A (#60, `sphere-shader-parameter-reyes`): the fill does not remove it (1 of 11), consistent with a separate defect.

**Verdict on the hypothesis**: confirmed for the reyes scenes (arrival order of equal-depth opaque fragments), rejected for the raytrace scenes, where a nondeterministic shading input is the cause. Neither is a sampler defect: the sample positions and keys are identical in good and bad runs.

**Proposed fixes** (test-first, each through TEST-APPROVAL, not started):

1. Cause 1: make the offset lane's `Ps - dir`, and any other derivative operand of a lane that did not hit, defined (mirror the real lane, so the derivative is 0), instead of left unwritten. The test is a shading-level check with `threadMemory` pre-filled with two different bytes that must give identical lighting; plus `ctest -L determinism-sweep` on the scenes above.
2. Cause 2: break equal-z ties by a deterministic key (for example the grid's `stableId`, then the micropolygon index) instead of by arrival order. The test is a scene with two coplanar opaque triangles of different colour that overlap, rendered at `-t:1` and `-t:4`.

### T084b and T084c: investigation and tests first (nothing fixed)

**T084b: the missed derivative lane, confirmed at the lane tags.** Instrumenting `transmission()` showed, for the real lanes with a huge `dTdv`, `tags[real] = 0` (active) and `tags[numRealVertices + 2i + 1] = 1` (inactive): the dv offset lane is deactivated inside the `solar()` block of `raydistant.sl`, so `Ps - dir` is never written for it, and `dvVector()` differences against unwritten memory. (In a fresh process that memory is zero and the derivative is `-D / dv`, about -1000, a wrong value even when it is repeatable.) The same four-line pattern is in the eight `duFloat/DuFloat/dvFloat/DvFloat/duVector/DuVector/dvVector/DvVector` functions.
- Test: `ShadingParity_DerivativeInactiveLanes` (`tests/shading_parity/test_derivative_inactive_lanes.cpp`, label `shading_parity;determinism`). It builds a `SHADING_2D` state with 4 real vertices, fills the inactive offset lanes with 0x00 and with 0x5A, and for `jitDuFloat`, `jitDvFloat`, `jitDuVector` and `jitDvVector` requires: identical results between the fills, derivative 0 on the real lanes with an inactive offset lane, and the plain difference quotient on the others (control).
- Red today: 10 checks pass (the controls and the active lanes) and 8 fail (the bit-identity and the zero-derivative check, once per function). The failure is for the right reason: the poison content reaches the result.
- Design note for the approval: the test fixes the value of a missed lane to a derivative of 0 (the lane mirrors the real lane), as agreed.

**T084c: why the two z values are exactly equal.** Instrumenting the opaque test of the stochastic hider (`drawPixel()`) for every equal-z rejection at `-t:1`:

| Scene | Equal-z rejections | On a degenerate micropolygon (two coincident corners, `u = +-0`) |
|---|---|---|
| blobby-dent-reyes | 3 | 3 |
| blobby-repeller-heights-reyes | 3 | 3 |
| blobby-spiral-reyes | 118 | 118 |
| bunny-reyes | 15 | 15 |
| blobby-blend-far-reyes | 5 | 5 |
| csg-hero-displaced-reyes | 0 | none: not a tie (third cause) |

- *Mechanism.* A triangle is diced as a quad grid whose apex cell has `v2 == v0`. `checkPixel()` computes `u = aleft / (aleft + aright)` with `aleft` the area of the zero-length edge `v2 -> v0`, which is exactly 0, so `u = 0` for every sample in the cell, and `z = (v0.z (1-u) + v1.z u)(1-v) + (v2.z (1-u) + v3.z u) v` collapses to `v0.z` (and the colour to `v0`'s colour). The cells of two triangles that meet at one mesh vertex therefore have the bit-identical z of that vertex wherever they overlap on screen. In `blobby-dent-reyes` the tie sample `(236.3516, 327.3817)` lies in two slivers of two different objects that share the vertex `(236.18037, 327.67706, 31.64327)`; both report `u = +-0` and z = 31.6432686 exactly.
- *Verdict.* Not a shared edge, not duplicate or overlapping geometry. It is a depth (and colour) interpolation defect on degenerate micropolygons, present in the reyes and the zbuffer hider alike (`zbufferQuad.h` has the same `checkPixel()`). **It is GitHub #62** and gets its own `fix/` branch; the tie-break below is independent of it.
- *Cost of the tie-break.* It needs the winner's key per sample (about 8 bytes in `CPixel`, written on every accepted fragment); the compare itself runs only on `z == pixel->z`.
- Tests: `DeterminismTies_reyes` and `DeterminismTies_zbuffer` (`examples/rib/tests/ties-coincident-{reyes,zbuffer}.rib`): two coincident opaque triangles (red and blue), identical vertices, hence a tie at every sample of the overlap; 128x128, 16x16 buckets, `Option "sampling" "sampler" ["sobol"]`, float RGBA; `-t:1` against eight `-t:4` renders must be bit-identical. Red today: every comparison differs, 6654 to 9048 of 65536 values (both hiders), while `-t:1` against `-t:1` is identical.

- Next decision for the maintainer: which of A, B and C are fixed before Phase 8 (and where: they are not sampler defects as far as these runs show, except that class C only shows with jitter on), and whether the red scenes stay registered meanwhile. They are registered, red, and unmodified.

### T084b and T084c: results

**Rule used for "moves".** The default (Legacy) sampler is random, so two renders of the *unchanged* code already differ: in two identical baseline runs of the 400 reference, parity and visual tests that report a block difference, 348 report a different value (typically by 0.1 to 5, up to 12.9 for `Visual_spotlight-reyes` and 15625 of 500000 for `Parity_depthdefault`). A reference test therefore cannot show a movement smaller than that. To get an exact measure, every one of the 413 swept scenes plus the two tie scenes was also rendered at `-t:1` under the sobol sampler (deterministic) before and after each fix, and the outputs compared bit for bit (`tiffdiff`); the reference tests that render a scene which moved are then listed with their old and new values. All 718 tests of the reference/visual/parity/csg set (the full suite without the sweep and the new tests) passed after each fix; no threshold or reference was touched.

#### T084b (the missed derivative lane)

- **Fix.** In `src/libshader/shading/executeMisc.cpp`, the `SHADING_2D` branch of all eight functions (`duFloat`, `DuFloat`, `dvFloat`, `DvFloat`, `duVector`, `DuVector`, `dvVector`, `DvVector`) reads the offset lane through `inactive ? real lane : offset lane`, with `inactive` the lane tag of the offset lane (`tags[numRealVertices + 2i]` for the du lane, `+ 1` for the dv lane). An inactive offset lane thus gives a derivative of exactly 0. `ShadingParity_DerivativeInactiveLanes` is green (18 of 18 checks).
- **Proof (temporary `ralloc` fill, reverted).** Each scene rendered 6 times at the thread count shown for each of four fills (none, 0x00, 0x55, 0xFF), 24 renders, compared with the first: **0 differ** for `csg-hero-plain-sphere-raytrace`, `csg-hero-displaced-raytrace`, `csg-tolerance-inert-raytrace`, `csg-hero-raytrace-800` (all `-t:4`); `csg-hero-translucent-raytrace` and `csg-hero-two-shaders-raytrace` (`-t:1` and `-t:4`); `csg-procedural-operand-raytrace` (`-t:1`, `-t:4`); `csg-procedural-operand-inline-raytrace` (`-t:1`); `csg-secondary-rays-reyes` and `csg-secondary-rays-raytrace` (`-t:4`). Before the fix the same test gave 12 of 15 (`csg-hero-plain-sphere-raytrace`), 11 of 11 (`csg-secondary-rays-reyes`) and similar.
- **Not fixed by it**, and why. `csg-hero-zbuffer-1600`, `csg-hero-stats-1600-reyes` and `csg-hero-reyes-1600`, `csg-hero-zbuffer-800`, `csg-offscreen-stats-reyes`, `csg-hero-partial-stats-reyes` still change their image when `ralloc` memory is filled with 0xFF (NaN), and only then (0x00 and 0x55 give the unfilled image). The cause is the same defect in grid shading: the derivative of a grid lane next to an inactive grid lane (the terminator of the `raydistant` light) reads the unwritten lane. Confirmed with a throw-away prototype that let an inactive grid lane take an active neighbour's value: with it the image under the 0xFF fill equals the unfilled image for `csg-offscreen-stats-reyes` and `csg-hero-zbuffer-800`. It was not kept: the grid rule is not covered by the approval (a lane inactive in an `if` block often still holds a valid value that existing shaders may rely on), so it needs its own decision.
- **Sweep entries.** Re-enabled: `csg-hero-translucent-raytrace`, `csg-hero-two-shaders-raytrace`, `csg-secondary-rays-reyes`, `csg-procedural-operand-raytrace`, `csg-procedural-operand-inline-raytrace` (all five green in both sweeps below). Kept disabled under #61: `csg-hero-zbuffer-1600` (10 of 31 renders differ from the first at `-t:1` and 31 of 31 at `-t:4`, so it is unstable beyond the 0xFF group) and `csg-hero-stats-1600-reyes` (fill-sensitive, above). `csg-procedural-operand-raytrace` had not differed in the unfilled runs; the fill shows it is fixed (0 of 23 at `-t:1`, 0 of 23 and 0 of 31 at `-t:4`).
- **What moved at `-t:1` under sobol:** 6 of 415 outputs, a few pixels each: csg-hero-raytrace-800 (3 pixels differ), csg-hero-reyes-1600 (4 pixels differ), csg-hero-two-shaders-raytrace (3 pixels differ), csg-offscreen-stats-reyes (1 pixels differ), csg-secondary-rays-raytrace (30 pixels differ), csg-secondary-rays-reyes (29 pixels differ).
- **Reference and parity tests that render a moved scene** (value = block-average difference, threshold in the last column; "old" is two runs of the unchanged code, which differ from each other by Legacy noise):

| Test | old run 1 | old run 2 | new | Threshold |
|---|---|---|---|---|
| Csg_Seam_hero_raytrace_800 | Passed | Passed | Passed | (pass/fail) |
| Csg_ToleranceInert | 2.0 | 2.31 | 2.83 | 20.0 |
| Csg_UntouchedRegion_hero | 2.16 | 1.53 | 1.42 | 20.0 |
| Parity_csg-hero-800-raytrace-reyes | 7.47 | 8.2 | 9.05 | 20.0 |
| Parity_csg-hero-800-raytrace-zbuffer | 7.45 | 7.3 | 7.34 | 20.0 |
| Csg_Seam_hero_reyes_1600 | Passed | Passed | Passed | (pass/fail) |
| Parity_csg-hero-1600-raytrace-reyes | 4.11 | 4.0 | 3.42 | 20.0 |
| Csg_PerOperandShader_hero_raytrace | Passed | Passed | Passed | (pass/fail) |
| Parity_csg-hero-two-shaders-raytrace-reyes | 8.66 | 8.56 | 6.8 | 20.0 |
| Csg_OffscreenStats_reyes | Passed | Passed | Passed | (pass/fail) |
| Csg_SecondaryRays_raytrace | Passed | Passed | Passed | (pass/fail) |
| Parity_csg-secondary-rays-raytrace-reyes | 7.64 | 8.75 | 10.23 | 20.0 |
| Csg_SecondaryRays_reyes | Passed | Passed | Passed | (pass/fail) |

#### T084c (the equal-depth tie)

- **Cause of the ties: GitHub #62** (depth and colour interpolation collapses on a degenerate micropolygon; see "T084b and T084c: investigation" above). It is not touched here; it gets its own `fix/` branch. The tie-break below removes the nondeterminism whatever the cause, so the ties of #62 and genuine ties (coincident surfaces) are handled alike.
- **Fix.** New shared header `src/ri/hiders/rasterTie.h` (`CTieKey` = the object's `stableId`, then the micropolygon or point index; `tieKeyLess`; `RASTER_TIE_KEY`). The reyes hider (`stochasticQuad.h`, `stochasticPoint.h`, `CPixel::tie` in `stochastic.h`) and the zbuffer hider (`zbufferQuad.h`, `zbufferPoint.h`; three more floats per sample, `SAMPLES_PER_PIXEL` 4 to 7) use the same functions: a fragment of exactly equal depth wins when its key is smaller, and the winner's key is stored with the sample. The check in the unshaded pre-pass (`drawPixelCheck`) uses the same condition, so a grid that would win a tie is shaded. Two things beyond the macros were needed for the test to pass: the bucket loop in `reyes.cpp` culled an object whose nearest depth equals the bucket's maximum opaque depth (`zmin < maxDepth`), so the second of two tied objects never reached the sample; it is now `<=`. Equal keys keep the first arrival, as before.
- **Not covered.** The transparent paths (a fragment list ordered by depth) keep their arrival order for equal depths; no scene in the suite showed it.
- **Tests.** `DeterminismTies_reyes` and `DeterminismTies_zbuffer` are green (3 consecutive runs each); they were red with 6654 to 9048 of 65536 values differing.
- **Proof.** Fills as above, 8 renders per fill (32 per scene) at `-t:4`: 0 differ for `blobby-dent-reyes`, `blobby-repeller-heights-reyes`, `blobby-spiral-reyes`, `bunny-reyes`, `blobby-blend-far-reyes`, and still 0 for `csg-secondary-rays-reyes`, `csg-hero-translucent-raytrace`, `csg-procedural-operand-raytrace`, `csg-hero-plain-sphere-raytrace`.
- **What moved at `-t:1` under sobol (after T084b):** 42 of 415 outputs; the winner at tie samples changes, so a handful of pixels per scene, 181 and 185 for `blobby-spiral-reyes` and `-zbuffer`: blobby-blend-far-reyes:7, blobby-blend-far-zbuffer:4, blobby-blend-influencing-zbuffer:5, blobby-blend-merged-reyes:7, blobby-blend-merged-zbuffer:4, blobby-carved-colour-zbuffer:4, blobby-coloured-octahedron-apart-reyes:3, blobby-coloured-octahedron-reyes:2, blobby-coloured-octahedron-zbuffer:16, blobby-dent-reyes:7, blobby-hand-maxed-reyes:13, blobby-hand-maxed-zbuffer:17, blobby-hand-selective-reyes:15, blobby-hand-selective-zbuffer:12, blobby-hand-summed-reyes:8, blobby-hand-summed-zbuffer:3, blobby-max-union-zbuffer:3, blobby-mixed-fields-reyes:3, blobby-mixed-fields-zbuffer:4, blobby-motion-grow-reyes:2, blobby-motion-grow-zbuffer:11, blobby-motion-still-zbuffer:4, blobby-motion-translate-reyes:6, blobby-motion-translate-zbuffer:4, blobby-object-instance-reyes:25, blobby-object-instance-zbuffer:4, blobby-parse-smoke:4, blobby-repeller-heights-reyes:12, blobby-repeller-shaping-reyes:7, blobby-repeller-shaping-zbuffer:4, blobby-solid-texture-bent-reyes:8, blobby-solid-texture-bent-zbuffer:4, blobby-solid-texture-straight-reyes:4, blobby-spiral-reyes:181, blobby-spiral-zbuffer:185, blobby-tolerance-default-reyes:8, blobby-tolerance-inherited-reyes:6, blobby-tube-reyes:32, blobby-tube-zbuffer:6, bunny-reyes:35, ties-coincident-reyes:3792, ties-coincident-zbuffer:3748. The two tie scenes (3792 and 3748 pixels) are the new tests.
- **Reference and parity tests that render a moved scene** (old = after T084b, new = after T084c; same noise caveat; the Legacy `Visual_*-zbuffer` and `Visual_blobby-*` tests move by up to 0.5, the parity tests by up to 3.7):

| Test | old | new | Threshold |
|---|---|---|---|
| Parity_blobby-blend-far-reyes-raytrace | 3.05 | 2.3 | 20.0 |
| Parity_blobby-blend-far-reyes-zbuffer | 1.3 | 2.69 | 20.0 |
| Visual_blobby-blend-far-reyes | 2.45 | 2.5 | 20.0 |
| Visual_blobby-blend-far-zbuffer | 0.2 | 0.25 | 20.0 |
| Parity_blobby-blend-influencing-reyes-zbuffer | 1.89 | 1.53 | 20.0 |
| Visual_blobby-blend-influencing-zbuffer | 0.23 | 0.38 | 20.0 |
| Parity_blobby-blend-merged-reyes-raytrace | 1.95 | 2.55 | 20.0 |
| Parity_blobby-blend-merged-reyes-zbuffer | 1.31 | 1.06 | 20.0 |
| Visual_blobby-blend-merged-reyes | 1.72 | 0.34 | 20.0 |
| Visual_blobby-blend-merged-zbuffer | 0.23 | 0.22 | 20.0 |
| Parity_blobby-carved-colour-reyes-zbuffer | 2.36 | 3.12 | 20.0 |
| Visual_blobby-carved-colour-zbuffer | 0.19 | 0.28 | 20.0 |
| Parity_blobby-coloured-octahedron-apart-reyes-raytrace | 3.78 | 4.45 | 20.0 |
| Parity_blobby-coloured-octahedron-apart-reyes-zbuffer | 3.64 | 3.55 | 20.0 |
| Visual_blobby-coloured-octahedron-apart-reyes | 0.0 | 0.66 | 20.0 |
| Parity_blobby-coloured-octahedron-reyes-raytrace | 5.05 | 5.86 | 20.0 |
| Parity_blobby-coloured-octahedron-reyes-zbuffer | 6.03 | 2.86 | 20.0 |
| Visual_blobby-coloured-octahedron-reyes | 0.14 | 4.16 | 20.0 |
| Visual_blobby-coloured-octahedron-zbuffer | 0.19 | 0.61 | 20.0 |
| Parity_blobby-dent-reyes-raytrace | 2.95 | 3.42 | 20.0 |
| Parity_blobby-dent-reyes-zbuffer | 2.98 | 2.98 | 20.0 |
| Visual_blobby-dent-reyes | 2.17 | 0.95 | 20.0 |
| Parity_blobby-hand-maxed-reyes-raytrace | 3.72 | 3.86 | 20.0 |
| Parity_blobby-hand-maxed-reyes-zbuffer | 3.48 | 2.83 | 20.0 |
| Visual_blobby-hand-maxed-reyes | 0.2 | 3.72 | 20.0 |
| Visual_blobby-hand-maxed-zbuffer | 0.2 | 0.5 | 20.0 |
| Parity_blobby-hand-selective-reyes-raytrace | 3.69 | 3.5 | 20.0 |
| Parity_blobby-hand-selective-reyes-zbuffer | 2.66 | 3.14 | 20.0 |
| Visual_blobby-hand-selective-reyes | 3.33 | 0.47 | 20.0 |
| Visual_blobby-hand-selective-zbuffer | 0.19 | 0.48 | 20.0 |
| Parity_blobby-hand-summed-reyes-raytrace | 3.58 | 3.7 | 20.0 |
| Parity_blobby-hand-summed-reyes-zbuffer | 2.55 | 2.36 | 20.0 |
| Visual_blobby-hand-summed-reyes | 0.17 | 0.45 | 20.0 |
| Visual_blobby-hand-summed-zbuffer | 0.22 | 0.22 | 20.0 |
| Parity_blobby-max-union-reyes-zbuffer | 2.05 | 1.67 | 20.0 |
| Visual_blobby-max-union-zbuffer | 0.2 | 0.25 | 20.0 |
| Parity_blobby-mixed-fields-reyes-raytrace | 3.77 | 3.67 | 20.0 |
| Parity_blobby-mixed-fields-reyes-zbuffer | 4.44 | 4.11 | 20.0 |
| Visual_blobby-mixed-fields-reyes | 0.27 | 3.38 | 20.0 |
| Visual_blobby-mixed-fields-zbuffer | 0.27 | 0.47 | 20.0 |
| Parity_blobby-motion-grow-reyes-raytrace | 3.16 | 2.95 | 20.0 |
| Visual_blobby-motion-grow-reyes | 0.05 | 0.08 | 20.0 |
| Visual_blobby-motion-grow-zbuffer | 0.12 | 0.16 | 20.0 |
| Parity_blobby-motion-still-reyes-zbuffer | 0.5 | 0.56 | 20.0 |
| Visual_blobby-motion-still-zbuffer | 1.17 | 1.17 | 20.0 |
| Parity_blobby-motion-translate-reyes-raytrace | 6.23 | 6.69 | 20.0 |
| Visual_blobby-motion-translate-reyes | 1.0 | 1.03 | 20.0 |
| Visual_blobby-motion-translate-zbuffer | 1.31 | 1.34 | 20.0 |
| Parity_blobby-object-instance-reyes-raytrace | 2.75 | 2.98 | 20.0 |
| Parity_blobby-object-instance-reyes-zbuffer | 2.72 | 2.2 | 20.0 |
| Visual_blobby-object-instance-reyes | 4.36 | 0.2 | 20.0 |
| Visual_blobby-object-instance-zbuffer | 0.19 | 0.2 | 20.0 |
| Visual_blobby-parse-smoke | Passed | Passed | (pass/fail) |
| Parity_blobby-repeller-heights-reyes-raytrace | 4.27 | 4.28 | 20.0 |
| Parity_blobby-repeller-heights-reyes-zbuffer | 1.86 | 2.08 | 20.0 |
| Visual_blobby-repeller-heights-reyes | 0.33 | 2.92 | 20.0 |
| Parity_blobby-repeller-shaping-reyes-raytrace | 4.19 | 4.14 | 20.0 |
| Parity_blobby-repeller-shaping-reyes-zbuffer | 2.09 | 2.03 | 20.0 |
| Visual_blobby-repeller-shaping-reyes | 3.33 | 0.66 | 20.0 |
| Visual_blobby-repeller-shaping-zbuffer | 0.27 | 0.53 | 20.0 |
| Parity_blobby-solid-texture-bent-reyes-raytrace | 3.23 | 3.44 | 20.0 |
| Parity_blobby-solid-texture-bent-reyes-zbuffer | 2.61 | 1.95 | 20.0 |
| Visual_blobby-solid-texture-bent-reyes | 1.33 | 2.14 | 20.0 |
| Visual_blobby-solid-texture-bent-zbuffer | 0.2 | 0.34 | 20.0 |
| Parity_blobby-solid-texture-straight-reyes-raytrace | 3.78 | 4.03 | 20.0 |
| Parity_blobby-solid-texture-straight-reyes-zbuffer | 2.7 | 1.75 | 20.0 |
| Visual_blobby-solid-texture-straight-reyes | 0.22 | 0.84 | 20.0 |
| Parity_blobby-spiral-reyes-raytrace | 6.12 | 5.88 | 20.0 |
| Parity_blobby-spiral-reyes-zbuffer | 4.47 | 4.41 | 20.0 |
| Visual_blobby-spiral-reyes | 0.22 | 1.12 | 20.0 |
| Visual_blobby-spiral-zbuffer | 0.23 | 1.23 | 20.0 |
| Parity_blobby-tolerance-default-reyes-raytrace | 2.97 | 2.64 | 20.0 |
| Parity_blobby-tolerance-default-reyes-zbuffer | 3.2 | 2.89 | 20.0 |
| Visual_blobby-tolerance-default-reyes | 2.66 | 2.19 | 20.0 |
| Parity_blobby-tolerance-inherited-reyes-raytrace | 4.95 | 2.98 | 20.0 |
| Parity_blobby-tolerance-inherited-reyes-zbuffer | 2.69 | 2.67 | 20.0 |
| Visual_blobby-tolerance-inherited-reyes | 0.2 | 0.73 | 20.0 |
| Parity_blobby-tube-reyes-raytrace | 3.69 | 3.58 | 20.0 |
| Parity_blobby-tube-reyes-zbuffer | 5.22 | 5.22 | 20.0 |
| Visual_blobby-tube-reyes | 2.92 | 3.45 | 20.0 |
| Visual_blobby-tube-zbuffer | 0.16 | 0.59 | 20.0 |
| Visual_bunny-reyes | 5.22 | 5.02 | 20.0 |

#### Sweep, twice, and the re-checks

- **Sweep** (`ctest -L determinism-sweep -j4`, 409 enabled tests; the four disabled entries are the two of #60 and the two left under #61): run 1 **406 pass, 3 fail** (`csg-hero-displaced-reyes`, `csg-hero-reyes-1600`, `csg-hero-two-shaders-reyes`); run 2 **406 pass, 3 fail** (`csg-hero-partial-stats-reyes`, `csg-hero-reyes-1600`, `csg-offscreen-stats-reyes`). Before T084b and T084c: 19 and 18 failing of 413. Every blobby and bunny scene and all the raytrace scenes are green in both runs; the failures are all in the CSG hero reyes family.
- **Time.** 620 s and 700 s, against 422 s for the first sweep of this phase. The machine was loaded (load average 40 to 44 throughout): `teapot-motion-raytrace`, which none of the changes touches, went from 115 s to 297 s. The 10-minute limit of FR-030 therefore needs a measurement on an idle machine before it is decided whether the sweep keeps its `determinism` label.
- **Re-check of the four scenes.**
  - `csg-hero-displaced-reyes`: still differs (1 render of 8 at `-t:4`, 11 pixels of about +-1e-3 on black pixels in the first column of a bucket, `x = 448`); it has no tie, the fill does not explain it, and the cause is not found. It stays red.
  - `blobby-blend-far-reyes`: green (0 of 31 under four fills, and in both sweeps).
  - `csg-offscreen-stats-reyes`, `csg-hero-partial-stats-reyes`: each failed once in the two sweeps, though the unfilled renders were identical 5 of 5; the only difference found is the 0xFF fill image (the grid derivative residual above). They stay red.

### T084d: the label split, the timings and the Parity measurement

**FR-030 compliance note.** The full sweep does not fit FR-030's 10-minute budget on this machine once the load is realistic, so it is opt-in, and a representative subset is in the default `determinism` label, as FR-030 and research M3 provide.
- `determinism-sweep`: every swept scene (413 registered, 403 enabled, 10 disabled under #60, #61 and #63), with no `visual` or `regression` label. **Run it before each merge and in the Linux validation (T105), explicitly**: `ctest -L '^determinism-sweep$' -j2`. A full run without it is `ctest -LE determinism-sweep`.
- `determinism` (default): 148 tests, 154 s and 171 s at `-j2`.
- Use the anchored label, `ctest -L '^determinism$' -j2`. `ctest -L determinism` is a regular expression and also selects every `determinism-sweep` test (448 tests in all, the whole sweep).

**What `determinism` holds.**
- 35 tests that are not sweep scenes: `DeterminismHarness_*` (8), `DeterminismTies_*` (2), `ColorDeterminism_*` (4), `DepthFilter_*` determinism tests (6), `ShadingParity_DerivativeInactiveLanes`, and, by label only, `Sampling_ThreadSweep_*` (4), `Sampling_DisplaceThreadSweep`, `Sampling_DisplaceRepeat` and `SamplingJitDeterminism_*` (8).
- 113 sweep scenes (the list is `DETERMINISM_DEFAULT_SCENES` at the end of `tests/visual/determinism-scenes.cmake`):
  - the **most demanding** scene of each family and hider that takes at most 10 s in the sweep (the family is the first word of the name, split further for `csg` and `teapot`, 107 family and hider groups); the cap keeps the set inside its budget, and within it the slowest scene is taken, not the fastest;
  - every scene that has exposed a defect: the tie scenes of #62 (`blobby-spiral-reyes`, `blobby-spiral-zbuffer`, `blobby-dent-reyes`, `blobby-repeller-heights-reyes`, `blobby-blend-far-reyes`, `bunny-reyes`), the missed-lane scenes of #61 (`csg-secondary-rays-raytrace`, `csg-secondary-rays-reyes`, `csg-hero-plain-sphere-raytrace`, `csg-hero-translucent-raytrace`, `csg-procedural-operand-raytrace`, `csg-procedural-operand-inline-raytrace`), and `sphere-random`, `sphere-trace` and `sphere-gather` in both shader formats;
  - no disabled entry.
- Labels only: no test body, threshold or reference changed. `add_determinism_sweep` gives `determinism-sweep` only (no `visual` or `regression`), so `-L visual` and `-L regression` no longer select the sweep; the default scenes get `determinism` added.
- **How to run what** (counts checked with `ctest -N`): the everyday full run is `ctest -LE determinism-sweep` (721 tests); the default determinism set is `ctest -L '^determinism$' -j2` (148); the opt-in full sweep is `ctest -L '^determinism-sweep$' -j2` (413, 403 enabled). `-L visual` went from 822 to 409 tests and `-L regression` from 888 to 475 (the 413 sweep tests are gone from both).

**Timings at `-j2`** (through `cpujob.sh` with `nice -n 19`, one job at a time, nothing else of mine running):

| Run | Tests | Result | Total time | Load average at start / end |
|---|---|---|---|---|
| `-L '^determinism$'`, run 1 | 148 | all pass | 153.9 s | 4.7 / 5.9 |
| `-L '^determinism$'`, run 2 | 148 | all pass | 171.4 s | 5.9 / 6.0 |
| `-L '^determinism-sweep$'`, first timing | 413, 403 enabled | all 403 pass | 606.7 s (10.1 min) | 15.7 (just after a build) / 7.3 |
| `-L '^determinism-sweep$'`, second | 403 | all pass | 644.7 s (10.7 min) | 7.3 / 5.9 |
| `-L '^determinism-sweep$'`, after the split | 403 | all pass | 727.0 s (12.1 min) | 6.0 / 7.9 |

The summed test time of the sweep is 1213 s, so `-j2` cannot do better than about 607 s (the slowest single test is `teapot-motion-raytrace`, 130 s). The load average during a `-j2` sweep is 6 to 9 (each test runs up to 4 threads or 2 processes).

**The earlier `-j4` figures.**
- First sweep of the phase: 421.9 s on a quiet machine, but it took the load average to 40 to 57.
- Full suite (1131 tests) at `-j4`: 476.5 s.
- After T084b and T084c, the sweep at `-j4`: 620.5 s and 700.4 s, with a load average of 40 to 44; `teapot-motion-raytrace` went from 115 s to 297 s though no change touches it.
- The full suite at `-j4`: 862 s with a load average of 57, 686 s with the sweep at `-j2`.
- `-j4` is too much for this fanless M2, so the sweep runs at `-j2`.

**`Parity_csg-secondary-rays-raytrace-reyes`, 10 runs each** (61826b3, before both fixes, against 09a2202, with them; one job at a time through `cpujob.sh`; load average 4.5 to 9.8; the builds were run in sequence):

| Build | Mean | Std. dev. | Min | Max |
|---|---|---|---|---|
| 61826b3 | 9.12 | 1.00 | 8.12 | 10.67 |
| 09a2202 | 9.05 | 1.53 | 7.55 | 12.00 |

The difference of means is -0.07 with a standard error of 0.58 (t = -0.12), against a threshold of 20. **The shift is not real**: the earlier climb from 7.64 and 8.75 to 12.52 was Legacy noise, whose own spread is about 1.5.
