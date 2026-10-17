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
