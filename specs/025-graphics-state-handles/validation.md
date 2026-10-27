# Validation: Graphics State Handles

**Plan**: [plan.md](plan.md) | **Quickstart**: [quickstart.md](quickstart.md)

This file holds the measured evidence, oldest first. The raw data for §0 is in [tools/](tools/).

## §0 Baselines on the starting tree, macOS (2026-10-26)

**Tree**:
- `build-025` was configured from the worktree at master `dfffba0`: Release, vcpkg
  (`arm64-osx-openrender`), Homebrew bison.
- At the time, the worktree held only the spec documents and the two new test files (§0.4, §0.5).
- No source under `src/` was changed. The measurement variants were built in a scratch copy
  outside the worktree.
- Every job ran through `~/.claude/bin/cpujob`, at `-j4` or lower.

### §0.1 Build

The clean configure and build succeeded (exit 0).

### §0.2 Architecture check

`ctest -L architecture` passed. The full report:

```
ALLOWED   DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-008 x 7 DB-010 x 35 DB-011 x 173
          DB-015 x 10 DB-017 x 78 DB-019 x 3 DB-021 x 1 DB-025 x 12 DB-026 x 3
METRIC    Geometry 78/19, Hiders 567/84, Infra 0/0, SceneInterface 6/6, Shading 0/0,
          Texturing 70/16, TOTAL 721/110 (all equal to metrics-baseline.txt)
RESULT    PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0
```

These are the "before" numbers for SC-001, SC-002 and SC-009: 7 DB-008 edges, and Graphics State
has no metric row.

### §0.3 Full suite

`ctest -LE determinism-sweep -j4`: **784/784 passed**, in 154 s (before the red `State_LightOrder` was in the build; the tasks stage re-takes the baseline total).

Graphics State has no `CRenderer::` metric row, which confirms the spec's "no link edges from State
sources".

### §0.4 Part A of the parameter equivalence (FR-007)

The test is `tests/shading_parity/test_shader_parameter_equivalence.cpp` (`ShadingParity_ShaderParameterEquivalence`).

**On `dfffba0`: 64 passed, 0 failed.**
- The test covers these cases:
  - none (before `RiWorldBegin`);
  - the world defaults;
  - surface only;
  - atmosphere over the default surface;
  - all three;
  - all three plus a light;
  - the light switched off;
  - displacement plus surface;
  - the surface cleared;
  - after `ResourceRestore`.
- `defaultsurface` and `defaultlight` were both bound.

This shows the oracle reproduces today's `findParameter` and `usedParameters`.

**Mutation (scratch copy)**: `findParameter` was changed to search surface before displacement.
Result: **60 passed, 4 failed** (`foundDefault("shared") == shared` in the four cases that bind both
slots). So the test fails on a build where the answer differs.

### §0.5 Light order (FR-025)

**The test**: `State_LightOrder` (`tests/unit/state/test_light_order.cpp`).

| Build | Result |
|---|---|
| `dfffba0` | **red**: 5 passed, **4 failed**. Failing: "inside AttributeBegin", "after a copy-on-write clone", and the two checks that follow them. Passing: same block, nested double push, after AttributeEnd |
| `dfffba0` plus the fix sketch (`tools/lightorder-fix-sketch.diff`), scratch copy | **green**: 9/9 |

**Impact scan**: `tools/lightorder_scan.sh`, with `tools/lightorder-probe.diff` applied in the
scratch copy only.
- **Coverage**: all **489** `examples/rib/**/*.rib`. 487 reached `RiWorldEnd`; the other 2 are
  archive operands with no world block.
- **Result**: **82 scenes** have a different light-order hash with the fix
  (`tools/affected-ribs.txt`). The list is in plan.md, "FR-025 affected scenes and references".
- **Multi-frame scenes**: 16 were re-run frame by frame (`tools/lightorder_frames.sh`). They add no
  scene to the list.
  - In one run or the other, the known `CPhotonHider` race (BUGS.md) cut short the second frame of
    some `photon-solar0-*` scenes.
  - Where both runs produced a frame, its hash is identical, except for scenes already in the list.
- **Procedurals**: the probe does not see objects expanded at render time. 7 scenes contain
  procedurals, and 2 of them have two or more light statements:
  - `csg-procedural-open-cylinder-raytrace` is in the list, because its parse-time objects differ;
  - `csg-procedural-missing-archive` is not.

  Light order inside their render-time expansions is unobserved; the phase 7 full suite covers
  it.

**The tests that render an affected scene** (`tools/affected-tests.tsv`): 139.
- 35 `Visual_*`, 29 `Parity_*`, 2 `Sampling_ThreadSweep_*`, and 1 `NotRequired_*` plus 1 `Perf_*`
  (manual, not run);
- 71 `DeterminismSweep_*`.

**The 66 runnable non-sweep tests, run on both builds**:

| Measurement | Starting tree | With the fix |
|---|---|---|
| tests passed | 66/66 | 66/66 |
| `MaxBlockAvgDiff` per test (`tools/affected-diff-compare.tsv`) | — | **identical for all 63 tests that print one** |
| output images, byte-for-byte (`tools/affected-bitwise.tsv`) | — | **95/95 bit-identical** |

**Conclusion**:
- The fix changes the order in which 82 scenes accumulate light, a float summation order.
- No 8-bit output image of any test changes on macOS.
- **No reference needs re-rendering.** Phase 7 re-checks this, and so does Linux (§4 of the
  quickstart).

**The 71 determinism-sweep tests with the fix**: these compare configurations within one tree, so
light order cannot fail them. Result with the fix (scratch copy, `-j2`): **71/71 passed**.

**Architecture check, re-run** after the two new test files were built into `build-025`: 21/21
passed, unchanged.

### §0.6 Timing of the slowest three visual scenes (SC-005)

The three slowest `visual`-labelled tests in §0.3 were each run 5 times, alone, on the starting
tree:

| Test | Runs (s) | Min | Median | Max |
|---|---|---|---|---|
| `Visual_teapot-motion-raytrace` | 20.62, 19.65, 19.54, 19.41, 19.55 | 19.41 | 19.55 | 20.62 |
| `Csg_Seam_freeform_nurbs` | 4.16, 4.17, 4.10, 4.11, 4.15 | 4.10 | 4.15 | 4.17 |
| `Csg_Seam_spherecube_reyes_1600` | 4.22, 4.24, 4.17, 4.23, 4.24 | 4.17 | 4.23 | 4.24 |

**SC-005 check after the change**: each median falls within its min–max band.

### §0.7 libshader_shading flat-namespace imports (spec 024)

`nm -m libshader_shading.1.dylib | grep 'dynamically looked up'`: **7**
(`tools/libshader-dynlookup-before-macos.txt`). The seven are:
- `CPhotonMap::lookup`;
- `CTexture3d::resolve`;
- `CGatherBundle::CGatherBundle()`;
- `CShadingContext::trace` and `traceEx`;
- the `CTraceBundle` and `CTransmissionBundle` vtables.

This is spec 024's value. After 025 the count must still be 7.

## §1 Implementation log (macOS, `build-025`)

**Base commit**: `dfffba0` (master when the branch was cut). **Worktree HEAD at Phase 1**: `a684aa7532bf27fe2355d8be57dce49dcfdd5092`. **Build tree**: `build-025` (Release, Unix Makefiles, vcpkg `arm64-osx-openrender`, Homebrew bison, `OPENRENDER_ENABLE_JIT=ON`).

**Expected-red rule**: `State_LightOrder` (4 of 9 checks failing on `dfffba0`) is the only allowed red test from Phase 2 until Phase 7. A red test from the known-flaky set is re-run alone before it is written off.

**Per-phase checkpoint results** (CP-a build, CP-b suite, CP-c architecture, CP-d image diff):

| Phase | CP-a build | CP-b suite (`-LE determinism-sweep -j4`) | CP-c architecture | CP-d image diff |
|---|---|---|---|---|
| 1 (baseline, §1.1, §1.4) | clean, exit 0 | 787 registered: 784 passed, 1 failed (`State_LightOrder`), 1 skipped, 1 disabled | 21/21; `ALLOWED DB-008 x 7`; PASS | baseline recorded: 729 images (§1.4); noise check empty |
| 2 | | | | n/a (no source change) |
| 3 | | | | |
| 4 | | | | |
| 5 | | | | |
| 6 | | | | |
| 7 | | | | n/a (output may change) |
| 8 | | | | |

**How to take CP-b/CP-d evidence** (learned in Phase 1):
- `ctest --output-on-failure` prints `MaxBlockAvgDiff` only for failing tests. The per-test output of every test is in `build-025/Testing/Temporary/LastTest.log`, which **the next `ctest` run overwrites** (CP-c and T005 included). Copy it right after the suite: `cp build-025/Testing/Temporary/LastTest.log build-025/logs/<label>.LastTest.log`.
- Then `tools/image_manifest.sh diffs build-025/logs/<label>.LastTest.log | diff - tools/baseline-diffs.txt` must print nothing (CP-d's `MaxBlockAvgDiff` check).
- Stamp (`image_manifest.sh stamp build-025`) before the suite; run `image_manifest.sh diff ... baseline-image-noise.txt` straight after it and before any other ctest run.

### §1.1 Baseline on the unchanged tree (T003, T004)

- Tree: HEAD `a684aa7`, no `src/` change. The tree holds the two new test files (`State_LightOrder` and the equivalence test), nothing else new.
- **CP-a**: `cpujob 025-base-build cmake --build build-025 --config Release -j4`: clean (exit 0, nothing to rebuild).
- **CP-b**: `cpujob 025-base-suite ctest --test-dir build-025 -LE determinism-sweep -j4 --output-on-failure`, real time 155 s.
  - **787 tests registered**: 784 passed, **1 failed**, 1 skipped, 1 disabled. ctest reports "99% tests passed, 1 tests failed out of 786" (the disabled test is not counted) and exits 8, because of the failure.
  - **Failed**: `State_LightOrder` (5 passed, 4 failed: "inside AttributeBegin", "after a copy-on-write clone", "Illuminate off then on moves the light to the end", "after a ResourceRestore round trip"). This is the expected red.
  - **Not run, both present before this spec**:
    - `ShadingParity_IlluminationHookAllFalse` (Disabled): disabled in `tests/shading_parity/CMakeLists.txt` until the fix for GitHub #71 lands.
    - `LibShader_Format_rslo_lower_minor` (Skipped by the test itself).
  - No known-flaky test was red. `ShadingParity_ShaderParameterEquivalence` passed; `ShadingParity_AmbientAccumulation` passed.
  - The total is not comparable with §0.3's 784: this run includes `State_LightOrder`, the disabled test and the skipped one (784 + 3 = 787 registered).
  - `MaxBlockAvgDiff` lines saved as `tools/baseline-diffs.txt` (389 tests, 397 values, `name<TAB>value`, sorted), taken from `LastTest.log`.
- **CP-c**: `ctest -L architecture`: 21/21 passed. `archcheck --source-dir . --build-dir build-025 --config tests/architecture` (through `cpujob`):
  - `ALLOWED DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-008 x 7 DB-010 x 35 DB-011 x 173 DB-015 x 10 DB-017 x 78 DB-019 x 3 DB-021 x 1 DB-025 x 12 DB-026 x 3`
  - `METRIC TOTAL refs 721 (baseline 721) distinct 110 (baseline 110)`; Shading 0/0, Infra 0/0
  - `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`
- **T004**: `tools/image_manifest.sh record build-025 > tools/baseline-images.sha256`: **724 images**, all `*.tif` under `build-025/tests`. (Superseded by §1.4: the baseline was re-recorded after T006a, with 729 images.)
- **Coverage gap of the manifest** (it scanned `<build-dir>/tests` only, as specified): four `imager_*_test.tif` files that `ImagerBackgroundRender`, `ImagerDualDisplayRender`, `ImagerParamOverrideRender` and `ImagerRegressionRender` write at the root of `build-025` were not hashed. No `*.png` or `*.exr` is produced. Closed by T006a (§1.4).

### §1.2 Tool check

`image_manifest.sh` was exercised on a scratch directory first (stale file ignored, a changed file reported `DIFFERS`, a deleted one `MISSING`, a new one `INFO`, noise list honoured, exit 1/0), then on the real data (§1.3). A `diffs <LastTest.log>` sub-command was added to produce `MaxBlockAvgDiff` tables; it reproduces `baseline-diffs.txt` byte for byte from the T003 log.

### §1.3 Run-to-run noise on the unchanged tree (T005)

Stamp, CP-b a second time (169 s, same result: only `State_LightOrder` red, same skipped/disabled), then `image_manifest.sh diff tools/baseline-images.sha256 build-025`.

- **`MaxBlockAvgDiff`**: all 389 tests print identical values in both runs.
- **Images**: 723 of 724 identical. **1 differs: `tests/imager/ri.tif`** (not empty, unlike the plan stage's 95/95; that number covered only the 95 images of the affected scenes). Cause, measured:
  - `ImagerExecutionTests` (sha256 starting `df0fe4d3`) and `ImagerGuardTests` (starting `b60bae7e`) both render with the default display name `ri.tif` in the same working directory, `build-025/tests/imager`. Under `-j4` the last writer wins.
  - Each, run alone, is deterministic: 7 of 7 runs identical for `ImagerExecutionTests`; `ImagerGuardTests` was run once alone and gave `b60bae7e`.
  - So it is a shared scratch file, not a test product with run-to-run variance.
- At the time `tools/baseline-image-noise.txt` listed `tests/imager/ri.tif`; T006a (§1.4) removed the cause and the line.

### §1.4 T006a: the `ri.tif` race fixed, the manifest widened, the baseline re-recorded

Maintainer decision 2026-10-26 after Checkpoint 1.

- **Fix** (`tests/imager/CMakeLists.txt` only; no test source, no assertion changes): `ImagerGuardTests` runs in its own working directory, `build-025/tests/imager/guard` (created at configure time with `file(MAKE_DIRECTORY ...)`). Its default `ri.tif` now lands in `tests/imager/guard/ri.tif`; `ImagerExecutionTests` keeps `tests/imager/ri.tif`. The two tests no longer share an output file.
- **Manifest**: `image_manifest.sh record` also hashes `<build-dir>/imager_*_test.tif` (the four `Imager*Render` outputs at the build root), when newer than the stamp.
- **Re-recorded on the fixed tree** (reconfigure and rebuild, clean; then stamp, CP-b, `record`): **729 images** = the earlier 724, the 4 build-root `imager_*_test.tif`, and `tests/imager/guard/ri.tif`. Suite: same result as §1.1 (787 registered, only `State_LightOrder` failed, one skipped, one disabled), 145 s.
- **`MaxBlockAvgDiff`**: byte-identical to the earlier `baseline-diffs.txt` (389 tests), so that file is unchanged.
- **Noise check** (stamp, CP-b again, then `image_manifest.sh diff tools/baseline-images.sha256 build-025`): `manifest 729 images, fresh 729, noise-listed 0; 0 differing, 0 missing, 0 not in manifest`, exit 0. The `MaxBlockAvgDiff` table of the second run equals the first. **The noise list is empty** (`tools/baseline-image-noise.txt` holds only a comment).

## §2 Phase 2: tests first (T007–T021), macOS

No file under `src/` changed in this phase (`git diff --stat -- src` is empty; T020's scratch edit was reverted). Tests and CMake only.

### §2.1 The tests written

| Test | Kind | State at the end of Phase 2 |
|---|---|---|
| `State_Isolation` | State-only | red by **build failure**, `DISABLED`, defined only with `OPENRENDER_STATE_ONLY_TESTS=ON` |
| `State_LastBound` | State-only | red by build failure, same |
| `State_PatternTable` | State-only | red by build failure, same |
| `State_ChannelRegistry` | State-only | red by build failure, same |
| `State_LightOrderRestore` | State-only | red by build failure (and by assertion once it builds), same; enabled in Phase 7 (T071) |
| `State_VocabularyConsistency` | Consumer | red by build failure, `EXCLUDE_FROM_ALL` + `DISABLED` |
| `ShadingParity_ShaderParameterEquivalenceDirect` | Consumer | red by build failure, `EXCLUDE_FROM_ALL` + `DISABLED` |
| `ShadingParity_ShaderParameterBxdfIgnored` | Consumer | red by build failure, `EXCLUDE_FROM_ALL` + `DISABLED` |
| `ShadingParity_ShaderParameterEquivalence` (part A) | refactor (T015) | **green, 64 passed, 0 failed** |
| `RibOut_ShaderStatementOrder` | characterization | **green**: statement order preserved in 3 interleavings |
| `State_LightOrder` | written earlier | **red, 5 passed / 4 failed** (the only red in the default build) |

The fixtures of part A moved into `tests/shading_parity/shader_parameter_fixtures.h` (T015); part A's cases and assertions are unchanged, and the oracle's answers are read through a small access struct (`SParameterAccess`) that part A points at `CAttributes` and part B at the `libshader::` functions.

### §2.2 Red evidence, State-only tests (scratch build `build-025-red`, option ON)

`build-025-red` was configured like `build-025` (generator, `CMAKE_BUILD_TYPE`, toolchain, `VCPKG_*`, bison, deployment target) plus `-DOPENRENDER_STATE_ONLY_TESTS=ON`, and deleted afterwards.

`cmake --build build-025-red --target state_only_sources` **fails to compile** the State sources on the narrow include path:
```
src/ri/state/attributes.h:33:10: fatal error: 'shader.h' file not found
src/ri/state/options.h:30:10: fatal error: 'blobbyField.h' file not found
```
(`attributes.cpp` and `pl.cpp` stop at `shader.h`; `options.cpp` stops at `blobbyField.h`. A fatal error ends a translation unit, so `dsply.h` (`options.h:34`) and `photonMap.h` are shown by a one-line probe with the same include path:)
```
#include "shader.h"       -> fatal error: 'shader.h' file not found
#include "photonMap.h"    -> fatal error: 'photonMap.h' file not found
#include "dsply.h"        -> fatal error: 'dsply.h' file not found
#include "blobbyField.h"  -> fatal error: 'blobbyField.h' file not found
#include "stateHandles.h" -> fatal error: 'stateHandles.h' file not found
```
The five test executables depend on that library, so their own objects were compiled one by one (`make test_state_*.o` in the test directory):
```
test_state_isolation      attributes.h:33:10: fatal error: 'shader.h' file not found
test_state_last_bound     attributes.h:33:10: fatal error: 'shader.h' file not found
test_state_pattern_table  attributes.h:33:10: fatal error: 'shader.h' file not found
test_state_light_restore  attributes.h:33:10: fatal error: 'shader.h' file not found
test_state_channel_registry  tests/unit/state/test_state_channel_registry.cpp:37:10: fatal error: 'channelRegistry.h' file not found
```
(`attributes.h` is the first include of four of them, so `shader.h` is the first error; `stateHandles.h`, `patternTable.h`, `bindBxdf` and the rest are absent too, as the probe above shows for `stateHandles.h`.)

Archcheck was **not** run in the scratch tree.

### §2.3 Red evidence, Consumer tests (`build-025`)

```
test_state_vocabulary
  tests/unit/state/test_state_vocabulary.cpp:38:10: fatal error: 'stateVocabulary.h' file not found
test_shading_parity_shader_parameter_equivalence_direct
  tests/shading_parity/test_shader_parameter_equivalence_direct.cpp:33:10: fatal error: 'libshader/shading/shaderParameters.h' file not found
test_shading_parity_shader_parameter_bxdf_ignored
  tests/shading_parity/test_shader_parameter_bxdf_ignored.cpp:33:10: fatal error: 'libshader/shading/shaderParameters.h' file not found
```
The bxdf test stops at the missing header first. With a throwaway stub of that header (scratchpad, not in the tree) the same file reports the next errors, and nothing else:
```
test_shader_parameter_bxdf_ignored.cpp:75:16: error: no member named 'bindBxdf' in 'CAttributes'
test_shader_parameter_bxdf_ignored.cpp:75:25: error: use of undeclared identifier 'ShaderGroupRef'
test_shader_parameter_bxdf_ignored.cpp:76:28: error: no member named 'bxdf' in 'CAttributes'
```
The Direct test and the vocabulary test, compiled against stubs of the missing headers, are clean under `-Wall -Wextra` (the vocabulary test also under `-Werror`), so the only thing missing is the API.

### §2.4 Green by design and the one red

- `ShadingParity_ShaderParameterEquivalence`: **64 passed, 0 failed** after the T015 refactor.
- `RibOut_ShaderStatementOrder`: passed. Three RIB inputs (different orders of the seven shader statements, interleaved with `Color`, `Opacity`, `ShadingRate`, `Attribute`, and in one input inside an `AttributeBegin` block, one rebinding a slot twice) are written by the RIB writer, and the statement sequence (keyword plus shader name) of each equals the input's; writing the output again gives the same sequence. It is a characterization test: green on the starting tree by design.
- `State_LightOrder`: **red, 5 passed / 4 failed**, the same four checks as in §1.1.

### §2.5 `DISABLED` tests and ctest's exit status

`ctest -L 'state|shading_parity'` in `build-025` lists, under "The following tests did not run": `ShadingParity_IlluminationHookAllFalse` (Disabled, before this spec; GitHub #71), `ShadingParity_ShaderParameterEquivalenceDirect`, `ShadingParity_ShaderParameterBxdfIgnored` and `State_VocabularyConsistency` (all Disabled). With `State_LightOrder` excluded (`-E '^State_LightOrder$'`) the exit status is **0**: ctest does not count disabled tests as failures. The only failure of the full run is `State_LightOrder`.

### §2.6 Mutation check (T020)

In a scratch edit of `src/ri/state/attributes.cpp` (`findParameter`: surface searched before displacement), after a rebuild, `ShadingParity_ShaderParameterEquivalence` gives **60 passed, 4 failed** (all four are `foundDefault("shared") == shared` in the cases that bind both slots), as at the plan stage (§0.4). The edit was reverted with `git checkout -- src/ri/state/attributes.cpp`; `git diff --stat -- src` is empty.

### §2.7 Checkpoint 2 (CP-a to CP-c; CP-d not needed, no source changed)

- **CP-a**: `cmake --build build-025 --config Release -j4`: clean under `-Werror` (the new tests included). The `ld` duplicate-library warnings are the linker's, as before.
- **CP-b**: `ctest -LE determinism-sweep -j4`, 150 s: **791 registered: 785 passed, 1 failed (`State_LightOrder`), 4 disabled, 1 skipped.** The 785 are the 784 of §1.1 plus `RibOut_ShaderStatementOrder`. No known-flaky test was red. The `MaxBlockAvgDiff` table (`image_manifest.sh diffs` of the saved `LastTest.log`) equals `tools/baseline-diffs.txt`.
- **CP-c**: `ctest -L architecture`: 21/21. `archcheck` report: `ALLOWED DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-008 x 7 DB-010 x 35 DB-011 x 173 DB-015 x 10 DB-017 x 78 DB-019 x 3 DB-021 x 1 DB-025 x 12 DB-026 x 3`, `METRIC TOTAL refs 721 (baseline 721) distinct 110 (baseline 110)`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`. With the option off no State-only object is required.

## §3 Phase 3: the parameter hook (T022–T029), macOS

**Change**: `CAttributes::checkParameters()` and `findParameter()` now forward to the hook State owns (`src/ri/state/shaderParameterHooks.{h,cpp}`); the logic moved verbatim to `libshader::usedShaderParameters` / `findShaderParameter` in `src/libshader/shading/shaderParameters.{h,cpp}`; `rendererShadingServicesInit()` installs `&libshader::shaderParameterHooks()` and `rendererShadingServicesShutdown()` removes it. `pl.cpp`, `blobby.cpp` and `rib.y` are not edited. The slots are still raw `CShaderInstance *`. The Direct test (part B) is enabled.

### §3.1 Tests that turned green

- `ShadingParity_ShaderParameterEquivalence` (part A): **64 passed, 0 failed**, unchanged.
- `ShadingParity_ShaderParameterEquivalenceDirect` (part B): **69 passed, 0 failed** (part A's 64 cases, plus 5 checks on the published and the installed hook pointers).
- **Mutation (T027)**: in a scratch edit of `src/libshader/shading/shaderParameters.cpp` (`findShaderParameter` searching surface before displacement), after a rebuild: part A **60 passed, 4 failed** and part B **65 passed, 4 failed**. Both go red on the same four cases. The edit was reverted by restoring the saved copy (`diff` empty).
- Part A's "installation order" case (the fresh `RiBegin` block, before `RiWorldBegin`, answers `PARAMETER_N`) passes, so the hook is installed before the first `CAttributes` is built.

### §3.2 Guards (T028)

- **libshader_shading flat-namespace imports**: `nm -m libshader_shading.1.dylib | grep 'dynamically looked up'`: **7**, the same seven symbols as `tools/libshader-dynlookup-before-macos.txt` (`CPhotonMap::lookup`, `CTexture3d::resolve`, `CGatherBundle::CGatherBundle()`, `CShadingContext::trace` and `traceEx`, the `CTraceBundle` and `CTransmissionBundle` vtables).
- **Architecture**: unchanged (§3.3). The hook adds no include or link edge from State; `shaderParameters.cpp` includes only State and Shading headers.
- **ribVector and ribOut** build and link (part of CP-a).

### §3.3 Checkpoint 3 (CP-a to CP-d)

- **CP-a**: `cmake --build build-025 --config Release -j4`: clean under `-Werror`.
- **CP-b**: `ctest -LE determinism-sweep -j4`: **791 registered**: 786 passed, 1 failed (`State_LightOrder`, 5 passed / 4 failed), 3 disabled (`ShadingParity_IlluminationHookAllFalse` before this spec, `ShadingParity_ShaderParameterBxdfIgnored`, `State_VocabularyConsistency`), 1 skipped (`LibShader_Format_rslo_lower_minor`, before this spec). The 786 are Phase 2's 785 plus `ShadingParity_ShaderParameterEquivalenceDirect`. No known-flaky test was red.
- **CP-c**: `ctest -L architecture`: 21/21. Report: `ALLOWED DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-008 x 7 DB-010 x 35 DB-011 x 173 DB-015 x 10 DB-017 x 78 DB-019 x 3 DB-021 x 1 DB-025 x 12 DB-026 x 3`, `METRIC TOTAL refs 721 (baseline 721) distinct 110 (baseline 110)`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`, no `STALE` line.
- **CP-d**: stamp, CP-b, then `image_manifest.sh diff tools/baseline-images.sha256 build-025 tools/baseline-image-noise.txt`: `manifest 729 images, fresh 729, noise-listed 0; 0 differing, 0 missing, 0 not in manifest`. The `MaxBlockAvgDiff` table (`image_manifest.sh diffs` of the saved `LastTest.log`) equals `tools/baseline-diffs.txt` for all 389 tests.

## §4 Phase 4: handles, maps and the vocabulary (T030–T054), macOS

**Change**: Graphics State holds opaque handles (`src/ri/state/stateHandles.h`: `CShaderGroupRefTarget`, `CMapRefTarget`, `TStateRef`, `ShaderGroupRef`, `MapRef`) and its own vocabulary (`stateVocabulary.h`). `CShaderInstance` derives from `CShaderGroupRefTarget`, `CPhotonMap` from `CMapRefTarget`. The five shader slots of `CAttributes` are private handles read through accessors and written through `bindSurface/Displacement/Atmosphere/Interior/Exterior` and `setDefaultSurface`; `globalMap`/`causticMap` are public `MapRef`; `COptions::imager` is a `ShaderGroupRef`; `CActiveLight::light` is a non-owning `CShaderGroupRefTarget *`. `attributes.{h,cpp}` and `options.{h,cpp}` no longer include `shader.h`, `photonMap.h`, `dsply.h` or `blobbyField.h`. Shading reads through the inline `shaderRefs.h` (`shaderInstanceOf`, `lightInstanceOf`), Hiders through `photonMapOf`. The prepend loops of the copy constructor and `restore()` are unchanged (Phase 7).

### §4.1 The seven DB-008 violations (T030)

With the 7 `DB-008` lines (and their header) deleted from `tests/architecture/allowlist.txt` and nothing else changed, `ctest -L architecture` failed with exactly these 7 `VIOLATION` lines, all sourced in `src/ri/state`:
```
VIOLATION include src/ri/state/attributes.cpp:33 -> src/ri/hiders/photonMap.h  [GraphicsState -> Hiders]
VIOLATION include src/ri/state/attributes.h:33 -> src/libshader/shading/shader.h  [GraphicsState -> Shading]
VIOLATION include src/ri/state/attributes.h:33 -> src/ri/core/shader.h  [GraphicsState -> Shading]
VIOLATION include src/ri/state/options.h:34 -> src/display/dsply.h  [GraphicsState -> Display]
VIOLATION include src/ri/state/options.h:39 -> src/libshader/shading/shader.h  [GraphicsState -> Shading]
VIOLATION include src/ri/state/options.h:39 -> src/ri/core/shader.h  [GraphicsState -> Shading]
VIOLATION include src/ri/state/options.h:30 -> src/ri/geometry/blobbyField.h  [GraphicsState -> Geometry]
```
No other violation appeared. After the code change the same check passes (§4.5).

### §4.2 Compiler-guided sweep and the includes it needed (T049)

The tree did not compile from T036 to T049, as documented. The first sweep (`-k`) showed only lost transitive includes; one more build was clean. Added, and only these:

| File | Added include | Why |
|---|---|---|
| `src/ri/state/pl.h` | `common/memory.h`, `common/riVocabulary.h` | State's own file; infrastructure (`CMemPage`, `RI_S`/`RI_T`) that `shader.h` used to bring in |
| `src/ri/state/options.cpp` | `common/os.h` | State's own file; infrastructure (`osAvailableCPUs`, `OS_MAX_PATH_LENGTH`, `osModuleExtension`) |
| `src/ri/render/renderer.h` | `dsply.h`, `shader.h` | renderer core: `options.h` and `attributes.h` no longer pull in the display ABI and Shading |
| `src/ri/render/renderer.cpp`, `rendererContext.cpp` | `shaderRefs.h` | renderer core |
| `src/ri/render/rendererContext.cpp`, `rendererDisplay.cpp` | `stateConversions.h` (new, core-private) | the `ParameterType` / `EDisplayParameterType` and `BLOBBY_ORDER_*` / `EBlobbyOrder` conversions, with a `static_assert` per pair |
| `src/libshader/shading/shading.cpp`, `execute.cpp`, `executeMisc.cpp`, `shaderParameters.cpp` | `shaderRefs.h` | Shading |
| `tests/imager/test_imager_guard.cpp`, `test_imager_options.cpp` | `libshader/shading/shaderRefs.h` | consumers |

**No Geometry, Texturing or Scene Interface unit needed a Shading, Hiders or Display header.** `pl.cpp` is untouched (the include went into `pl.h`), so the T052 "callers not edited" check holds. Hiders (`photon.cpp`, `radiance.cpp`, `irradianceDispatch.cpp`) already included `photonMap.h`.

**Approval**: the maintainer approved Checkpoint 4 on 2026-10-26, including the adaptations of `CSG_InteriorExteriorSelection` and of the imager guard and options tests (bound and read through the new accessors; no assertion changed).

### §4.3 State-only tests enabled (T050, T051)

- `OPENRENDER_STATE_ONLY_TESTS` is ON by default and was set explicitly on the `build-025` reconfigure; `state_only_sources` is part of `all`. The State sources now **compile on the narrow include path** (T019's errors are gone) and link against `openrendercommon` only.
- `State_Isolation`: **198 passed, 0 failed.** `State_VocabularyConsistency`: all 7 pairs agree. Both enabled; `State_LastBound`, `State_PatternTable`, `State_ChannelRegistry`, `State_LightOrderRestore` stay `EXCLUDE_FROM_ALL` + `DISABLED`.
- **Mutation C1** (scratch edit): a deleted manual `attach()` kept in the copy constructor (`if (surfaceSlot) surfaceSlot.get()->attach();`). `State_Isolation` goes red on the count checks, for example `refs(*shaders[i]) == 3: got 4, expected 3` and `CCountingShader::live == 0: got 1`. Reverted (saved copy restored, `diff` empty).
- **Mutation SC-007**: `EBlobbyOrder::AppNote = 2` in `stateVocabulary.h`. `State_VocabularyConsistency` goes red and names the pair: `FIRST DRIFTED PAIR: EBlobbyOrder::AppNote = 2, but BLOBBY_ORDER_APPNOTE = 1`. Reverted (`diff` empty).

### §4.4 Greps and diffs (T052)

- SC-003: `grep -nE '#include *"(.*/)?(shader|photonMap|dsply|blobbyField|shading)\.h"' src/ri/state/*` returns nothing.
- `git diff --stat dfffba0 -- src/display/dsply.h src/ri/geometry/blobbyField.h`: empty (FR-008).
- `git diff --stat dfffba0 -- src/ri/parse/rib.y src/ri/output src/ri/texture/texture3d.cpp src/ri/geometry/blobby.cpp src/ri/state/pl.cpp`: empty.

### §4.5 Checkpoint 4 (CP-a to CP-d)

- **CP-a**: clean under `-Werror`.
- **CP-b**: `ctest -LE determinism-sweep -j4`: **796 registered**: 788 passed, 1 failed (`State_LightOrder`, 5 passed / 4 failed), 6 disabled/skipped by design: `ShadingParity_IlluminationHookAllFalse` (before this spec, #71), `ShadingParity_ShaderParameterBxdfIgnored`, `State_LastBound`, `State_PatternTable`, `State_ChannelRegistry`, `State_LightOrderRestore` (disabled), `LibShader_Format_rslo_lower_minor` (skipped, before this spec). The 788 are Phase 3's 786 plus `State_Isolation` and `State_VocabularyConsistency`. Named in the checkpoint: `photonmap_FrameCacheRelease`, `photonmap_TwoFrameSession`, `photonmap_ObjectDefinitionRelease`, the six `Imager*` tests, `CSG_InteriorExteriorSelection`, `State_Isolation`, `State_VocabularyConsistency`, `ShadingParity_ShaderParameterEquivalence` and `…Direct` all passed. No known-flaky test was red.
- **CP-c**: `ctest -L architecture` 21/21. `archcheck` report: `ALLOWED DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-010 x 35 DB-011 x 173 DB-015 x 10 DB-017 x 78 DB-019 x 3 DB-021 x 1 DB-025 x 12 DB-026 x 3` (**no `DB-008`**), `METRIC TOTAL refs 721 (baseline 721) distinct 110 (baseline 110)`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`, no `STALE`, no Graphics State source in any edge. The `CONSUMER` lines are identical to Phase 3's; the `CYCLE` lines that went through the DB-008 edges are gone (three representative cycle paths changed, all `[allowlisted]`, `failing-cycles=0`).
- **Research R11 (first run with the State-only objects in the tree)**: archcheck did **not** mishandle the double-compiled State units. `loadMakefiles` found every object (no exit 2, `notbuilt=0`; `objects` rose from 258 to 265, the seven State-only objects). The raw edge totals rose (`edges` 2368 to 2392; `include` 1472 to 1491, `link` 896 to 901) because the second compile repeats production edges, but every deduplicated number the check reports (`ALLOWED` counts, `METRIC`, `CONSUMER`) is unchanged apart from the seven removed DB-008 lines. The staged `ri_config.h` copy in the build tree is not reported (no `UNRESOLVED` line for it; the 10 `UNRESOLVED` lines are the same 10 as before).
- **CP-d**: stamp, CP-b, `image_manifest.sh diff`: `manifest 729 images, fresh 729, noise-listed 0; 0 differing, 0 missing, 0 not in manifest`. The `MaxBlockAvgDiff` table equals `tools/baseline-diffs.txt` for all 389 tests.
- **libshader_shading flat-namespace imports**: still **7**.

## §5 Phase 5: the last-bound record and the pattern table (T055–T062), macOS

**Change**: `CAttributes` gains the private `bxdf` slot (`bxdf()`, `bindBxdf()`), the record `EMaterialBinding { None, Surface, Bxdf }` with the single query `governingMaterial()`, and the public `CPatternTable patterns` (`src/ri/state/patternTable.h`, header-only, copy-on-write over a shared vector). `bindSurface` and `bindBxdf` set the record even for an empty handle; `bindDisplacement/Atmosphere/Interior/Exterior` and `setDefaultSurface` leave it alone (FR-024). The copy constructor carries the record and shares the table through the memberwise copy; `restore(shading=true)` assigns the `bxdf` slot, the record and the table; `restore(shading=false)` leaves them. The `bxdf` slot is not read by `usedShaderParameters` or `findShaderParameter`. No RIB statement was added.

**Approval**: the maintainer approved Checkpoint 5 on 2026-10-27, including the fixture correction in `ShadingParity_ShaderParameterBxdfIgnored` (`dPdv` to `time`/`dtime`; no assertion changed).

### §5.1 Tests enabled and their results

- `State_LastBound`: **45 passed, 0 failed.**
- `State_PatternTable`: **52 passed, 0 failed.**
- `ShadingParity_ShaderParameterBxdfIgnored`: **97 passed, 0 failed** (after the fixture correction below).
- Still green: `ShadingParity_ShaderParameterEquivalence` (64), `ShadingParity_ShaderParameterEquivalenceDirect` (69), `RibOut_ShaderStatementOrder`.

**Fixture correction in the bxdf test (surprise)**: the test's own self-check `bxdfUniqueBits != 0` failed on its first run, because the bxdf fixture read `dPdv`, and another fixture (the displacement shader's `calculatenormal(P)`) already sets that bit; the bxdf's bits were `0x00cc2000`, all present in the others' union `0x46df7437`. The fixture now reads `time` and `dtime`, which no other fixture or default shader sets. Only that string and the comment in `test_shader_parameter_bxdf_ignored.cpp` changed; no assertion, no threshold. The self-check did its job: without it the mutation below would have gone unnoticed.

### §5.2 Mutations

- **FR-024**: in a scratch edit `setDefaultSurface` also set the record to `Surface`. `State_LastBound` went red on the FR-024 checks (`attributes.governingMaterial() == EMaterialBinding::None`, and `over.governingMaterial() == EMaterialBinding::Bxdf`; 43 passed, 2 failed). Reverted.
- **bxdf leak**: in a scratch edit `usedShaderParameters` also ORed in the bxdf slot's `requiredParameters()`. `ShadingParity_ShaderParameterBxdfIgnored` went red on `after.directUsed == before.directUsed`, `after.storedUsed == before.storedUsed` and `(after.directUsed & bxdfUniqueBits) == 0` in every case that binds a shader. Reverted.
- **Pattern table, append**: `define` appended instead of replacing. `State_PatternTable` red (`table.size() == 3: got 4`, position and shadowing checks). Reverted.
- **Pattern table, copy-on-write**: `define` skipped the copy of shared storage. `State_PatternTable` red (an inner definition leaked into the outer block and the source: `found(outer, "only-inside") == nullptr`, `source.patterns.size() == 2: got 3`). Reverted.
- All reverts restored saved copies (`diff` empty). Note on tooling: after one revert, the incremental build did not recompile the header-only test object, so the second pattern-table mutation first reproduced the first one's failures; I deleted that object and re-ran, which gave the genuine copy-on-write failures above. Every checkpoint build below ran after the reverts, and the green results show no mutation left in the binaries.

### §5.3 FR-012 and FR-023 (T058)

`git diff --stat dfffba0 -- src/ri/parse src/ri/output` is empty (no grammar change, no `Bxdf` or `Pattern` statement, ribOut unchanged), and `RibOut_ShaderStatementOrder` passes.

### §5.4 Checkpoint 5 (CP-a to CP-d)

- **CP-a**: clean under `-Werror`.
- **CP-b**: `ctest -LE determinism-sweep -j4`: **796 registered**: 791 passed, 1 failed (`State_LightOrder`, 5 passed / 4 failed), 3 disabled, 1 skipped. Not run: `ShadingParity_IlluminationHookAllFalse`, `State_ChannelRegistry`, `State_LightOrderRestore` (disabled) and `LibShader_Format_rslo_lower_minor` (skipped). The 791 are Phase 4's 788 plus `State_LastBound`, `State_PatternTable` and `ShadingParity_ShaderParameterBxdfIgnored`. No known-flaky test was red.
- **CP-c**: `ctest -L architecture` 21/21. The `ALLOWED`, `METRIC`, `RESULT` and `CONSUMER` lines are identical to Phase 4's: no new line, no `STALE`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`.
- **CP-d**: `image_manifest.sh diff`: `manifest 729 images, fresh 729, noise-listed 0; 0 differing, 0 missing, 0 not in manifest`; the `MaxBlockAvgDiff` table equals `tools/baseline-diffs.txt` for all 389 tests.
- libshader_shading flat-namespace imports: 7.

## §6 Phase 6: the channel registry moves to Graphics State (T063–T070), macOS

**Change**: `CChannelRegistry` (`src/ri/state/channelRegistry.{h,cpp}`) owns the display channels with the old storage and order: the five built-ins (`rgb` 3/0, `rgba` 4/0, `a` 1/3, `z` 1/4, `rgbaz` 5/0), then declared channels. `declare(const char *)` is `CRenderer::declareDisplayChannel(const char *)` moved unchanged (same error strings, `CODE_SYSTEM`, order, ownership); `declare(CVariable *)`, `retrieve()` (non-const result), `resetUsage()` and `forEach(fn(channel))` follow the contract; the destructor deletes each channel once through the ordered list. `beginChannelRegistry()` / `endChannelRegistry()` / `channelRegistry()` hold the session instance. `variableLookup.{h,cpp}` gained the declaration hook (`TVariableDeclaration`, `installVariableDeclaration`, `declareVariableThroughHook`). In the core, `initDeclarations()` calls `beginChannelRegistry()` and installs the captureless `declareVariableAdapter`; `shutdownDeclarations()` uninstalls the hook at the top and calls `endChannelRegistry()` at the old position (after the global ids, before the declared variables). `declareDisplayChannel` (both), `resetDisplayChannelUsage` and the two statics are gone; the three callers use `channelRegistry()` directly. `CRenderer::retrieveDisplayChannel` stays declared in `renderer.h` and is defined **out of line** in `rendererDeclarations.cpp`. `channelRegistry.cpp` is in `ri_sources` and in `ribOut_sources`. `texture3d.cpp` is untouched.

**Approval**: the maintainer approved Checkpoint 6 on 2026-10-27, including the one-check correction in `State_ChannelRegistry` (the built-ins' offset is compared against `sampleStart`, not `outType`).

### §6.1 State_ChannelRegistry (T068)

- **First run: 98 passed, 5 failed**, all five `channel->outType == offset` on the built-in channels. That was a defect in my Phase 2 test, not in the registry: the "offset" of a built-in (0, 0, 3, 4, 0) is the constructor's fourth argument, which `CDisplayChannel` stores in `sampleStart`; `outType` (the fifth argument) is `-1` for the built-ins, as before the move. The check now reads `sampleStart` (`tests/unit/state/test_state_channel_registry.cpp`, one line; nothing else in the test changed).
- **Now: 103 passed, 0 failed.** `State_ChannelRegistry` is enabled (the library's `channelRegistry.cpp` is part of `state_only_sources`).
- **Allocator debug run (T012)**: `MallocScribble=1 MallocGuardEdges=1 ctest -R '^State_ChannelRegistry$'` through `cpujob`: **103 passed, 0 failed, exit 0**.
- **Mutation (C1)**: `resetUsage()` skipping the last channel: `channel->sampleStart == -1: got 7, expected -1`; 102 passed, 1 failed. Reverted (saved copy restored, `diff` empty).
- **Mutation, double delete** (to show the balance check bites): a second `delete` of a channel in the destructor. On this macOS allocator the plain run already aborts (`SIGTRAP`), and the run with `MallocScribble=1 MallocGuardEdges=1` aborts (`Subprocess aborted`). Reverted.
- The stale incremental build seen in Phase 5 did not recur: each mutation rebuild recompiled `channelRegistry.cpp.o` (a `.cpp` change), and the green results afterwards show no mutation left behind.

### §6.2 Greps and object check (T069)

- `grep -nE 'declaredChannels|displayChannels|declareDisplayChannel|resetDisplayChannelUsage' src/ri/render/*`: nothing.
- `retrieveDisplayChannel` appears in `renderer.h` (declaration), `rendererDeclarations.cpp` (the out-of-line definition) and `texture3d.cpp:119` (the unchanged caller).
- `git diff --stat dfffba0 -- src/ri/texture/texture3d.cpp`: empty.
- `nm -C` on a `texture3d.cpp.o`: `U CRenderer::retrieveDisplayChannel(char const*)`, and no `channelRegistry` or `CChannelRegistry` symbol.

### §6.3 Checkpoint 6 (CP-a to CP-d)

- **CP-a**: clean under `-Werror` (ribOut and ribVector included).
- **CP-b**: `ctest -LE determinism-sweep -j4`: **796 registered**: 792 passed, 1 failed (`State_LightOrder`, 5 passed / 4 failed), 3 not run (`ShadingParity_IlluminationHookAllFalse` disabled before this spec for #71; `State_LightOrderRestore` disabled until Phase 7; `LibShader_Format_rslo_lower_minor` skipped before this spec). The 792 are Phase 5's 791 plus `State_ChannelRegistry`. `Parity_aov` and `Parity_transparency-matte-aov` passed, as did every other test; no known-flaky test was red.
- **CP-c**: `ctest -L architecture` 21/21. The `ALLOWED`, `METRIC`, `RESULT` and `CONSUMER` lines are identical to Phase 5's: no new line, no `STALE`. The allowlist keeps `DB-010 link src/ri/texture/texture3d.cpp -> CRenderer::retrieveDisplayChannel` and `DB-021 include src/ri/texture/texture3d.cpp -> src/ri/state/displayChannel.h`, neither reported stale, and no Texturing to Graphics State link appears.
- **CP-d**: `image_manifest.sh diff`: `manifest 729 images, fresh 729, noise-listed 0; 0 differing, 0 missing, 0 not in manifest`; the `MaxBlockAvgDiff` table equals `tools/baseline-diffs.txt` for all 389 tests.
- libshader_shading flat-namespace imports: 7.

## §7 Phase 7: light order (T071–T078), macOS

**Change**: `src/ri/state/attributes.cpp` gains the file-local `copyLightList()`: a new list of the same (non-owning) entries in the source's order, built with a tail pointer (O(n), no reversal). The copy constructor (attribute push, copy-on-write clone) and `restore(shading)` (which first frees its own entries) use it in place of the two prepend loops. `addLight` (append, duplicate suppression) and `removeLight` are unchanged. `State_LightOrderRestore` is enabled.

**Approval**: the maintainer approved Checkpoint 7 on 2026-10-27.

### §7.1 Red evidence before the fix (T071)

On the tree with Phase 6 only (the prepend loops still in place):
- `State_LightOrder`: **5 passed, 4 failed**: "inside AttributeBegin", "after a copy-on-write clone", "Illuminate off then on moves the light to the end", "after a ResourceRestore round trip".
- `State_LightOrderRestore`: **13 passed, 3 failed**: "copy constructor", "restore(shading = true)", "copy after the switch". ("clone of a clone" passes, because two reversals cancel; that is why the direct test exists.)

### §7.2 After the fix (T073)

- `State_LightOrder`: **9 passed, 0 failed.**
- `State_LightOrderRestore`: **16 passed, 0 failed.**

### §7.3 The affected tests (T074)

`ctest -R "$(cat tools/affected-regex.txt)" -j4 -V`: **66/66 passed** at unchanged thresholds. `tools/affected_diff_check.sh build-025/logs/025-p7-affected.log` compares every `MaxBlockAvgDiff` with the starting-tree column of `tools/affected-diff-compare.tsv`: `66 tests compared with the starting tree; 0 differing or missing` (63 print a diff, all identical; the other 3 print none, as before).

### §7.4 Images, full suite, determinism (T075, T076)

- **Full suite** (`ctest -LE determinism-sweep -j4`): **796 registered: 794 passed, 0 failed, 1 disabled, 1 skipped.** The disabled and the skipped test are the two present before this spec (`ShadingParity_IlluminationHookAllFalse`, #71; `LibShader_Format_rslo_lower_minor`). **Nothing is red**: `State_LightOrder` is now 9/9. The `MaxBlockAvgDiff` table (`image_manifest.sh diffs` of the saved `LastTest.log`) equals `tools/baseline-diffs.txt` for all 389 tests.
- **Images**: `image_manifest.sh diff tools/baseline-images.sha256 build-025 tools/baseline-image-noise.txt`: `manifest 729 images, fresh 729, noise-listed 0; 0 differing, 0 missing, 0 not in manifest`. That includes the 95 images of `tools/affected-bitwise.tsv`.
- **`ctest -L '^determinism$' -j2`**: 155/155 passed.
- **`ctest -L '^determinism-sweep$' -j2`** (once): 419/419 passed, 555 s.
- `DepthDeterminism_*` and `ColorDeterminism_*` (in the `^determinism$` set) passed, bit-identical.
- The procedurals expanded at render time (`csg-procedural-open-cylinder{,-inline}-raytrace`, `csg-procedural-missing-archive`), whose light order the plan-stage probe could not observe, passed: `Csg_Procedural_open_cylinder`, `Csg_Procedural_sphere`, `CSG_ProceduralOperand` and `Csg_procedural-missing-archive` in the full suite, and the `DeterminismSweep_csg-procedural-*` tests in the sweep, all with their diffs unchanged.

### §7.5 T077 outcome

**No reference changed.** Every `MaxBlockAvgDiff`, every image of the 729 and every test result equals the baseline; no reference was regenerated and no threshold touched. The plan-stage scan predicted this (82 scenes whose light-order hash changes, none with a changed 8-bit image). The check against the 82-scene list is by output: nothing observable changed anywhere in the suite, so nothing outside the 82 changed in a way the tests can see. The plan-stage probe itself (which hashes the order inside the renderer) was not re-run on the fixed tree.

### §7.6 Checkpoint 7 (CP-a to CP-c; CP-d is replaced by T075)

- **CP-a**: clean under `-Werror`.
- **CP-b**: see §7.4: everything green, no allowed exception.
- **CP-c**: `ctest -L architecture` 21/21; the `ALLOWED`, `METRIC`, `RESULT` and `CONSUMER` lines are identical to Phase 6's, no `STALE`.

## §8 Phase 8: documentation and the final macOS pass (T079–T086)

### §8.1 Documentation and registry (T079–T083)

- **`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`**:
  - DB-008 moved from Open to Resolved ("Resolved by spec 025", 2026-10-27), with the summary of what moved; its tracking line names #31 only.
  - DB-021 stays **Open**: tracking is spec 037 (#26) only, the evidence no longer says the registry is renderer-core state and now names the out-of-line forwarder to the State-owned registry, and a History bullet records what 025 did and did not touch.
  - DB-010's evidence notes that the forwarder forwards to the State registry (35 edges, unchanged).
  - DB-013 gained a History bullet: the unchecked light cast now lives in `lightInstanceOf()`.
  - DB-018 quotes no slot field read (it lists RSL function names), so it needed no wording change. No count in the Architecture-check section quotes the allowlist total.
- **Allowlist (T080)**: `git diff dfffba0 -- tests/architecture/allowlist.txt | grep -c '^+[A-Z]'` is **0**; the change is 9 deleted lines (the 7 DB-008 lines, their header and the blank line). `metrics-baseline.txt` is unchanged (no `NOTE metric … < baseline` was printed). The section-header counts in the allowlist were not changed by this spec and agree with the report's `ALLOWED` line (the platform-tagged DB-011 lines account for its 182 lines against 173 edges).
- **`DEVNOTES.md`**: a status-table entry for spec 025; macOS validation dated 2026-10-27, Linux pending until §9 is filled in.
- **`CLAUDE.md`** (edit authorized by the maintainer, 2026-10-26): Known gotchas item 24, the Recent Changes and Active Technologies entries, an Architecture paragraph ("Graphics State handles") and the `-L state` line in Testing. The "Spec-kit agent rules" section is byte-identical to `HEAD`.
- **Other docs (T083)**: no text in `docs/` describes the old slots or channel statics. `DEVNOTES_DETAILS/OSHADER_UPDATES.md` (the imager field) and `PATH-TRACING_HIDER.md` (the planned `bxdf` field) got one-line notes. No user-facing page changed.

### §8.2 No-JIT build (T084)

In a scratch directory inside the worktree (deleted afterwards): `cmake -B build-025-nojit -S . -DOPENRENDER_ENABLE_JIT=OFF` plus the same vcpkg/bison/deployment cache options as `build-025`; the full build is clean (`-Werror`); `ctest -L "architecture|state|shading_parity" -j4`: **46/46 passed**. Five JIT-only shading-parity tests (`UsedParametersOracle`, `OlWiring`, `PrintfJit`, `UniformConditionalJit`, `MatrixTransformJit`) report "Not Run (Disabled)": the existing "no LLVM JIT in this build" marker, not this spec's.

### §8.3 SC-005 timing (T085)

Each test 5 times alone through `cpujob`, on the finished tree:

| Test | Runs (s) | Min | Median | Max | Band of §0.6 (min–max) |
|---|---|---|---|---|---|
| `Visual_teapot-motion-raytrace` | 20.13, 19.68, 19.52, 20.76, 19.64 | 19.52 | 19.68 | 20.76 | 19.41–20.62: median inside |
| `Csg_Seam_freeform_nurbs` | 4.21, 4.13, 4.13, 4.20, 4.15 | 4.13 | 4.15 | 4.21 | 4.10–4.17: median inside |
| `Csg_Seam_spherecube_reyes_1600` | 4.26, 4.21, 4.32, 4.28, 4.27 | 4.21 | **4.27** | 4.32 | 4.17–4.24: **median 0.03 s (0.7%) above the band** |

The third median is outside the §0.6 band, so SC-005 as worded is not met by a hair. The machine ran slower later in the session than at the baseline (ten further runs of the same test gave 4.30–4.87 s, drifting with temperature), so I measured it differently as well: **interleaved A/B**, alternating this tree and a build of master's equivalent code (the `fix/jit-illumination-hook-false-if` build, which has none of this spec) five times each, back to back through `cpujob`:

| Test | This tree (s) | Master-equivalent build (s) |
|---|---|---|
| `Csg_Seam_spherecube_reyes_1600` | 4.35, 4.23, 4.41, 4.26, 4.31 (median 4.31) | 4.39, 4.27, 4.28, 4.32, 4.28 (median 4.28) |
| `Csg_Seam_freeform_nurbs` | 4.15, 4.13, 4.20, 4.13, 4.15 (median 4.15) | 4.19, 4.13, 4.18, 4.13, 4.16 (median 4.16) |

At the same time and temperature the two are indistinguishable (the difference is within the spread), so the excess against the §0.6 band is machine drift, not a slowdown from this spec. The maintainer decides whether SC-005 needs a literal re-measurement on a cold machine.

**Decision (maintainer, 2026-10-27): SC-005 is accepted as met.** The interleaved A/B above is the evidence: `Csg_Seam_spherecube_reyes_1600` at 4.27 s against the §0.6 band of 4.17–4.24 s is machine drift, because the same test measured back to back on this tree and on the master-equivalent build gives 4.31 s against 4.28 s. No cold-machine re-measurement is required.

### §8.4 Final macOS pass (T086)

**On the 025 tree** (Phase 7's results, nothing changed in `src/` or the tests since): see §7.4 and §7.6: CP-a clean; full suite 796 registered, 794 passed, 0 failed, 1 disabled (`ShadingParity_IlluminationHookAllFalse`, disabled on this branch's base until #71 landed) and 1 skipped (`LibShader_Format_rslo_lower_minor`, before this spec); architecture 21/21; `^determinism$` 155/155; sweep 419/419; the manifest and the `MaxBlockAvgDiff` table equal the baseline.

**On a scratch merge with master `bc261b1`** (which contains #71, `ShadingParity_IlluminationHookAllFalse` enabled). The merge was made in a throwaway detached worktree outside the 025 worktree (`git worktree add --detach … HEAD`, `git merge --no-commit --no-ff master`), never on the 025 branch; it had **no conflicts** (git auto-merged `shading.cpp` and `tests/shading_parity/CMakeLists.txt`). A fresh configure and build there:
- CP-a: clean under `-Werror`.
- CP-b: `ctest -LE determinism-sweep -j4`: **838 registered, 837 passed, 0 failed, 1 skipped** (`LibShader_Format_rslo_lower_minor`, before this spec). **Zero "Not Run (Disabled)"**; `ShadingParity_IlluminationHookAllFalse` ran and passed; `State_LightOrder` passed.
- `ctest -L 'state|shading_parity|rib_parse'`: 86/86 passed, **zero** "Not Run (Disabled)" lines (read from the run's output, not from `ctest -N`).
- CP-c: `ctest -L architecture` 21/21; the report has no `DB-008`, DB-021 present (`DB-021 x 1`), no `STALE`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`.
- `ctest -L '^determinism$' -j2`: 155/155.
- libshader_shading flat-namespace imports: 7.
- The scratch worktree was removed afterwards (`git worktree remove --force`, `git worktree prune`); `git worktree list` shows no `025-scratch-merge`, and the 025 branch is untouched at `d1a4cd3` plus the staged Phase 8 files.

Quickstart §3 greps (both trees): `git diff dfffba0 -- src/ri/texture/texture3d.cpp src/display/dsply.h src/ri/geometry/blobbyField.h` is empty; no `shader.h`, `photonMap.h`, `dsply.h`, `blobbyField.h` or `shading.h` include in `src/ri/state`.

## §9 Linux validation package (T087) — to be run by the maintainer

The agent has no Linux access. Linux green is required before the maintainer merges. When you report the results, append them under "§9.6 Results" and finalize the DEVNOTES entry.

### §9.1 What is on the Mac side

- Worktree: `/Volumes/Projects/Development/CLI/openrender-worktrees/025-graphics-state-handles` (branch `025-graphics-state-handles`, base `dfffba0`).
- `tools/025-linux-manifest.sha256`: a `sha256sum -c` manifest of **every file the spec changes** against `dfffba0` (96 files; added and modified, none deleted), except `validation.md` and the manifest itself, which change as results are recorded.
- No file is deleted or renamed, so nothing has to be removed on Linux.

### §9.2 Sync (zsh on the Mac; brace the variable, see memory `reference_zsh_host_colon_modifier.md`)

```bash
W=/Volumes/Projects/Development/CLI/openrender-worktrees/025-graphics-state-handles
H=juvenal@ubuntu-2404-desktop.v2-labs.io
R=/home/juvenal/Projects/Development/CLI/openrender            # the Linux checkout for the 025 tree
RB=/home/juvenal/Projects/Development/CLI/openrender-025-base  # a second tree: dfffba0, for the baseline

# The 025 tree: tracked files only, checksum compare, no -t (mtime trap, memory feedback_gcc13_werror_linux_validation.md)
git -C $W ls-files -z | rsync -rlpc --from0 --files-from=- "$W/" "${H}:${R}/"

# The baseline tree, from the base commit
git -C $W archive --format=tar dfffba0 | ssh "${H}" "mkdir -p ${RB} && tar -x -C ${RB}"

# Verify on the remote side; never trust the exit code of rsync
ssh "${H}" "cd ${R} && sha256sum -c specs/025-graphics-state-handles/tools/025-linux-manifest.sha256 | grep -vc ': OK$'"   # expect 0
```

Because `rsync -c` stamps only the changed files with the current time, an existing `build-025` would rebuild exactly those; use a **fresh** `build-025` anyway.

### §9.3 Build and run, 025 tree (Ubuntu 24.04, GCC 13, `-Werror`)

Through the Maestri "openRender Linux" agent (its classifier blocks building synced code until you run the command with `!`), or over ssh with the remote `~/.claude/bin/cpujob` at `-j2`:

```bash
cd ${R}
cpujob 025-cfg    cmake -B build-025 -S . -DCMAKE_BUILD_TYPE=Release -DINSTALL_SELFCONTAINED=OFF
cpujob 025-build  cmake --build build-025 -j4            # use nice -n 19 if not under cpujob
cpujob 025-arch   ctest --test-dir build-025 -L architecture --output-on-failure
cpujob 025-state  ctest --test-dir build-025 -L state --output-on-failure
cpujob 025-par    ctest --test-dir build-025 -L shading_parity -j4 --output-on-failure
cpujob 025-full   ctest --test-dir build-025 -LE determinism-sweep -j4 --output-on-failure
cpujob 025-det    ctest --test-dir build-025 -L '^determinism$' -j2 --output-on-failure
cpujob 025-sweep  ctest --test-dir build-025 -L '^determinism-sweep$' -j2 --output-on-failure   # once, ~10-12 min
cpujob 025-aff    ctest --test-dir build-025 -R "$(cat specs/025-graphics-state-handles/tools/affected-regex.txt)" -j4 -V > build-025/affected.log
specs/025-graphics-state-handles/tools/affected_diff_check.sh build-025/affected.log     # compare with the Linux baseline, see §9.4
```

Imports: `nm -u` on `libshader_shading` (the Linux counterpart of the macOS `nm -m` count): the set must equal spec 024's Linux value. Run it on the base tree too and diff the two.

### §9.4 The Linux baseline, on `dfffba0`, at the same time

Run the same commands in `${RB}` (a `build-base` directory, same configure options). Keep its `LastTest.log` copies (`cp build-base/Testing/Temporary/LastTest.log …` right after each run, because the next `ctest` overwrites it) and compare:

- the pass lists of `-LE determinism-sweep` and of the sweep: the 025 tree's must equal the base's, plus the new tests (`State_*`, `ShadingParity_ShaderParameterEquivalence{,Direct}`, `ShadingParity_ShaderParameterBxdfIgnored`, `RibOut_ShaderStatementOrder`) and `State_LightOrder` green (on the base tree it is not in the build; the new test files are in this tree only, so a base-tree run will not list them);
- **FR-025 on Linux**: every `MaxBlockAvgDiff` of the affected tests against the Linux base run (`affected_diff_check.sh` compares with the macOS starting-tree column; for Linux compare the two Linux logs with `diff <(tools/image_manifest.sh diffs A.LastTest.log) <(tools/image_manifest.sh diffs B.LastTest.log)`). **Any difference: list the scene, the old and new diffs and the reason (light order), and stop for the maintainer.** Never re-render and never touch a threshold.

### §9.5 Watch list for GCC 13 `-Werror`

- `-Wdeprecated-enum-enum-conversion` at the vocabulary conversion sites (`src/ri/render/stateConversions.h`, `rendererDisplay.cpp`, `rendererContext.cpp`); the code converts through `static_cast<int>`, so none is expected.
- `-Wnull-dereference` and `-Warray-bounds` around the pattern-table container (`patternTable.h`, a `std::shared_ptr<std::vector<…>>`) and the channel registry's `CArray`/`CTrie` use.
- `-Wsign-compare`/`-Wunused` in the new tests (`state_test_doubles.h`, `STATE_CHECK_EQ` casts through `long long`).
- `ctest -L architecture` passing with the platform-tagged allowlist lines unchanged (SC-001 on Linux): no `STALE`, no `VIOLATION`, no `DB-008` in `ALLOWED`.
- The `-L state` tests link only `openrendercommon`: a missing symbol there is a real State dependency.

### §9.6 Results

**Run 2026-10-27** (maintainer-approved), over ssh on `ubuntu-2404-desktop` (Ubuntu 24.04, 2 cores, GCC 13.3, cmake 3.28, LLVM 18.1.3, JIT on). Every job ran through the remote `cpujob` at `-j2`; the two trees ran one after the other.

**Trees.** Two fresh directories, never the existing checkout: `~/Projects/Development/CLI/openrender-025` (this worktree's tracked files, staged Phase 8 included; `git status` showed only the staged `validation.md`, no unstaged or untracked files) and `~/Projects/Development/CLI/openrender-025-base` (`git archive dfffba0`).
- `sha256sum -c tools/025-linux-manifest.sha256`: 96 lines, 0 non-OK.
- SHA-256 over every tracked file, Mac against Linux: 2927/2927 identical for the 025 tree (this file included: the staged version was synced), 2869/2869 for the base tree.

**Build** (`-DCMAKE_BUILD_TYPE=Release -DINSTALL_SELFCONTAINED=OFF`, fresh `build-025`/`build-base`): both clean under `-Werror`, **0 warnings, 0 errors**. Nothing on the §9.5 watch list fired.

| Run | 025 tree | Base tree (`dfffba0`) |
|---|---|---|
| `-L architecture` | 21/21 | 21/21 |
| `-L state` | 7/7 | no tests (label new in 025) |
| `-L shading_parity` | 30/30 | 27/27 |
| `-LE determinism-sweep` | 796: 788 passed, 7 failed, 1 disabled, 1 skipped | 785: 777 passed, 7 failed, 1 disabled, 1 skipped |
| `-L '^determinism$'` | 155/155 | 155/155 |
| `-L '^determinism-sweep$'` | 419/419 | 419/419 |
| affected (`-R affected-regex`) | 66/66 | 66/66 |

Disabled: `ShadingParity_IlluminationHookAllFalse` (#71, as on macOS). Skipped: `LibShader_Format_rslo_lower_minor`.

**Architecture (SC-001 on Linux).** The real-tree report is `RESULT PASS violations=0 stale=0` in both trees. The 025 tree's `ALLOWED` line has **no `DB-008`** (the base has `DB-008 x 7`), and `DB-021 x 1` is present. The other STALE/VIOLATION lines come from the archcheck fixtures, and they are the same in both trees.

**Pass lists.** For `-LE determinism-sweep`, the 025 tree is the base plus exactly 11 tests: `State_ChannelRegistry`, `State_Isolation`, `State_LastBound`, `State_LightOrder`, `State_LightOrderRestore`, `State_PatternTable`, `State_VocabularyConsistency`, `ShadingParity_ShaderParameterEquivalence`, `ShadingParity_ShaderParameterEquivalenceDirect`, `ShadingParity_ShaderParameterBxdfIgnored` and `RibOut_ShaderStatementOrder`. No test passes on the base and not on 025. **`State_LightOrder` is green.** The sweep and `^determinism$` pass lists are identical.

**Failures, all pre-existing on Linux.** The same 7 fail in both trees with identical signatures:
- `CoverageAnalytic_alpha-sides2-tjunction-r4_reyes` (pixel (99, 46) 0.875 against 0.9375)
- `TextureTile_RenderByteIdentical`
- `TextureTile_CacheByteIdentical`
- `TextureTile_CacheDefaultDirectory`
- `TextureTile_CacheStaleDetection`
- `TextureTile_CacheConcurrentWrite` (all five TextureTile tests: orender aborts, or the output differs from the reference)
- `LibShader_McompConsistency` (undefined `_ZTV19CTransmissionBundle` in `libshader_shading.so.1`)

**No failure appears only in the 025 tree.**

**FR-025 on Linux.** `image_manifest.sh diffs` over the affected run's `LastTest.log` gives 63 entries, **identical between the two trees** (`diff` empty). The full suite's 389 entries are identical too. Within each tree, the affected run and the full run agree for every affected test. `affected_diff_check.sh` against the macOS starting-tree column reports 6 platform differences, the same in both Linux trees, so they are not from this spec:
- `Parity_blobby-tolerance-inherited-reyes-raytrace` 0.88 → 0.91
- `Visual_blobby-repeller-shaping-raytrace` 2.25 → 2.27
- `Visual_csg-open-quadric-caps-reyes` 5.17 → 5.34
- `Visual_csg-open-quadric-caps-zbuffer` 4.08 → 4.16
- `Visual_nupatch-vase-trimmed-multiloop` 3.17 → 3.19
- `Visual_spotlight-raytrace` 6.70 → 19.92 (passes)

**Imports.** `nm -u -D libshader_shading.so.1.0.0`: 466 undefined symbols in each tree, and the sets are identical.

**Result: Linux is green for spec 025.** The only failures are the 7 pre-existing Linux ones, present identically on `dfffba0`. Logs: `build-025/logs/` and `build-base/logs/` in each remote tree (224M and 221M, kept).
