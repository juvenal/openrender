# Validation: Shading Services Routing

Results recorded while implementing spec 024. Each phase appends its section below "Baselines".

## Baselines

Recorded 2026-10-15 on the unchanged tree, before any source edit (tasks T001, T002).

### Base commit and build tree

- **Worktree HEAD**: `df77ded` (branch `024-shading-services-routing`), rebased onto master `c62334d`.
  The design documents were written and measured on master `690dbfc`; the macOS baselines below are
  re-captured on the new base. The Linux baselines were **not** re-captured and still describe `690dbfc`.
- **macOS build tree**: `build-024`, at the worktree root
  (`/Volumes/Projects/Development/CLI/openrender-worktrees/024-shading-services-routing/build-024`),
  Release, Makefile generator.
- **What master gained since `690dbfc`** (none of it touches the services, the illumination hooks or
  the photon hider): #50 and #48 tests, #54 (`keepOriginAbove()` in `src/common/random.h`, used by
  `executeMisc.cpp`, `shaderFunctions.h`, `shading.cpp` gather/occlusion origins and
  `irradianceDispatch.cpp`), #51 (`src/ri/geometry/patchLayout.*`, `surface.cpp`, `reyes.cpp`),
  and one allowlist line (the DB-006 `CReyes::drawGrid` signature gained a `CGridSnap const*`).

### R1: the no-argument `solar()` JIT gap (measured on `690dbfc`, not re-run)

- `.rslo` line: `solar #!Label2` (label alone).
- Bitcode `grep -ac` counts: `solar0_probe.slo` has `op_solar_begin` 0 and `op_solar_end` 1;
  `solar2_control.slo` has `op_solar_begin` 1 and `op_solar_end` 1.
- Red diff: `solar0-slo` against `solar0-rslo` gives MaxBlockAvgDiff **201.62**.
- Source: research.md R1; scenes in `tools/solar0-*.rib`, `tools/solar0_probe.sl`, `tools/solar2_control.sl`.

### R3: symbol baselines

- **macOS** (re-captured on `df77ded`, 2026-10-15): `nm -um libshader_shading.1.0.0.dylib`, filtered by
  "dynamically looked up" (the quickstart §0.1 command): **12** symbols. The set is **byte-identical**
  to `tools/libshader-dynlookup-before-macos.txt` (captured on `690dbfc`); `diff` is empty, so that file
  is unchanged. The 12:
  - `CGatherBundle::CGatherBundle()`, `CPhotonMap::lookup(float*, float const*, int)`;
  - `CRenderer::{getDSO, getFilter, getGlobalID, getStepFilter, retrieveVariable}`;
  - `CShadingContext::{trace, traceEx}`, `CTexture3d::resolve(int, char const**, int*, int*)`;
  - the vtables of `CTraceBundle` and `CTransmissionBundle`.
- **Linux** (measured on `690dbfc`, Ubuntu 24.04, GCC 13; not re-captured):
  `nm -D --undefined-only` gives **469** lines (`tools/024-libshader-undef-before-linux.txt`);
  direction-correct bindings give **91** lines (`tools/024-libshader-bindings-before-linux.txt`).
  Their strong-`U` intersection is the same 12 as macOS. The files are unchanged.

### R4: photon parity scene (measured on `690dbfc`, not re-run)

- macOS batch table, 30 runs each: default thread count **4/30** crashes (exit code 5, map absent);
  `-t:1` **0/30** crashes, diff **0.00** every run, `.gpm` 2 009 546 bytes every run.
- Linux at `-t:1` against the macOS reference: 30/30 runs, diff **2.91**, 0 crashes
  (`tools/024-photon-parity-linux-summary.txt`).
- D = 2.91, so the threshold = max(1, ceil(5.82)) = **6**.
- Empty-map margin: MaxBlockAvgDiff **255.00** (at least 18 = 3 x 6).
- The red `slo` run segfaults on both platforms (macOS: `EXC_BAD_ACCESS` at 0x0 in
  `CShadingContext::jitIlluminateBegin + 204`).

### Architecture (re-measured on `df77ded`)

- DB-001 allowlist lines: **18** (`grep -c '^DB-001 ' tests/architecture/allowlist.txt`).
- `metrics-baseline.txt` (committed): Shading `9 5`, TOTAL `742 117`.
- `archcheck` actual on this base: `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`.
  Shading refs 9 / distinct 5 (equal to the baseline). **TOTAL refs 749 against the committed baseline 742
  (+7: Geometry 78 vs 76, Hiders 586 vs 581, from #51)**, which archcheck reports as a WARNING, not a
  failure. TOTAL distinct 117 (equal).

### macOS full suite (T002)

- Build: `nice -n 19 cmake --build build-024 --config Release -j4`, exit 0, 59 s incremental. No
  compiler warnings from first-party code; the only warnings are the pre-existing Swift
  `orender-wire-macos` linker notes (built for macOS 13.3, linked at 13.0).
- `nice -n 19 ctest --test-dir build-024 -j4 --output-on-failure`: **100% tests passed, 0 tests failed out
  of 598**, 130 s. No known-intermittent test (`ImagerRegressionRender`, `Parity_transparency-matte-aov`,
  `Parity_depthdefault`, `ShadingParity_AmbientAccumulation`) failed on this run.
- `nice -n 19 ctest --test-dir build-024 -L architecture -j4`: **21/21 passed** (includes
  `Architecture_DomainBoundaries` and `Architecture_Determinism`).
- `determinism`-labelled tests (SC-006): **6/6 passed** (`DepthFilter_BackgroundExact_avg`,
  `DepthFilter_Determinism_avg`, `DepthFilter_BackgroundExact_mid`, `DepthFilter_Determinism_mid`,
  `ColorDeterminism_transparent`, `ColorDeterminism_opaque`).
- Later phases compare against these: 598 tests, 0 failures.

## Phase 2: services registered for the whole session

### Tests first (T003, T004), red state recorded 2026-10-15 on `bf957fd` plus the new test files

- `tests/shading_parity/recording_services.h`: `CRecordingServices` (forwards every method,
  counts `getFilter`, `getStepFilter`, `getGlobalID`, `retrieveVariable`) and `CStubServices`
  (sentinels: `RiBoxFilter`, `RiBoxStepFilter`, global ID 4242, a static `CVariable`).
- `tests/shading_parity/test_services_routing.cpp`, test `ShadingServices_Routing`, label
  `shading_parity`, one `RiBegin`/`RiEnd` session in the process. The build compiled and linked;
  `nice -n 19 ctest --test-dir build-024 -R ShadingServices_Routing --output-on-failure` fails,
  each case by assertion and with no crash:

  ```
  FAIL: original != nullptr                         (test_services_routing.cpp:52)   Case A
  FAIL: CShadingContext::defaultServices() == &wrapper  (test_services_routing.cpp:62)   Case C
  FAIL: CShadingContext::defaultServices() == nullptr   (test_services_routing.cpp:74)   Case B
  Results: 0 passed, 3 failed
  ```

  - A: services are still null after `RiBegin()` (registered only at the first `beginFrame()`).
  - C: `RiWorldBegin()`'s `beginFrame()` replaces the installed wrapper with the real implementation.
  - B: services stay registered after `RiEnd()`.
- Noise on stderr in this run, identical in the green state: `Failed to find shader
  "defaultsurface"/"defaultlight"` and `Failed to open out device "file"`, because the test's
  `ENVIRONMENT` is the one `ShadingParity_AmbientAccumulation` uses (`SHADERS` is the source
  `shaders/` directory, which holds no compiled objects, and `DISPLAYS` is unset). Case D (T024),
  a shader load before `RiWorldBegin`, needs `SHADERS` to point at `${CMAKE_BINARY_DIR}/shaders`.
- Later sessions in one process (spec Edge Cases) are covered by design, not by a second session in
  this test: `RiBegin` registers and `RiEnd` deregisters, and Cases A and B show both edges.

### Implementation (T005, T006) and verification (T007), 2026-10-15

- `src/ri/render/rendererShadingServices.cpp`: added `rendererShadingServicesShutdown()`
  (`CShadingContext::setDefaultServices(nullptr)`).
- `src/ri/render/renderer.cpp`: `rendererShadingServicesInit()` is called in `beginRenderer()`
  right after `initDeclarations()`; the call and its forward declaration are gone from
  `beginFrame()`; both forward declarations sit at the top of the file;
  `rendererShadingServicesShutdown()` is the last statement of `endRenderer()`.
- Build: `nice -n 19 cmake --build build-024 --config Release -j4`, exit 0, no first-party warnings.
- **`ShadingServices_Routing` is green**: Cases A, B and C pass (`Results: 3 passed, 0 failed`), where
  the red run failed all three.
- `-R 'Visual_killeroo|Imager'`: 9/9 passed (Imager shaders load before `WorldBegin`).
- **Full suite, run 1: 597 of 599 passed.** 599 = the 598 baseline plus the new test. Two failures,
  both under `-j4` load:
  - `Parity_transparency-matte-aov`: MaxBlockAvgDiff 28.70 against threshold 25.00, 1 block. This
    is on the known-intermittent list.
  - `Visual_sphere-shader-parameter-reyes`: 3 blocks over threshold 20, worst 77.09. This test is
    **not** on the known-intermittent list.
  - Re-run in isolation, both pass: 5 of 5 together, then `Visual_sphere-shader-parameter-reyes` alone
    60 of 60 with `--repeat until-fail:60`.
- **Full suite, run 2: 599/599 passed**, no failures at all (113 s).
- To look for a load-dependent flake in the sphere test, `-R 'Visual_sphere|Parity_' -j4` was run 8
  more times: 8 x 206/206 passed. The single failure of `Visual_sphere-shader-parameter-reyes` was
  not reproduced and its cause is not established; it is recorded as an unexplained one-off, not
  written off. The scene is a deterministic interpreter render and does not use the photon or
  illumination paths this spec changes.
- `-L architecture`: 21/21. `archcheck`: `RESULT PASS violations=0 stale=0 ...`; Shading refs 9
  (baseline 9), distinct 5, TOTAL distinct 117. The TOTAL refs warning (749 against 742, from #51) is
  unchanged by this phase.
- `-L determinism`: 6/6. `-L shading_parity`: 8/8 (7 existing plus `ShadingServices_Routing`).
