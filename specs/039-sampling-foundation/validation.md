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
