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

## Phase 3: US1, JIT light shaders honour the illumination hook

### Tests first (T008 to T013), red state recorded 2026-10-16 on `23dbe8b` plus the new test files

- **Shaders and scenes (T008, T009):** `shaders/photonparity_view.sl`, `shaders/solar0_light.sl`;
  `examples/rib/tests/{photon-parity,photon-parity-slo,solar0,solar0-slo}.rib`. The build compiled both
  shaders to `.rslo` and `.slo`.
- **References (T010)**, rendered with the unchanged interpreter path under `VISUAL_ENV`'s variables:
  - `references/photon-parity.tif`: `orender -t:1`, 2 009 546-byte map, exit 0. The R4 reference
    (`/tmp/024-pp-t1/work/ref.tif`) was gone, so it was regenerated on the post-Phase-2 tree with
    `tools/measure-photon-parity.sh build-024 1 <scratch> -t:1` (output in the session scratchpad, not
    `/tmp`). The regenerated run reproduces R4's numbers: `.gpm` 2 009 546 bytes, run-to-run diff
    0.00. **The new reference against that regenerated one: MaxBlockAvgDiff 0.00 and byte-identical
    (`cmp`).** This compares against a run on the post-Phase-2 tree, not R4's original one.
  - `references/solar0.tif`: `orender`, exit 0. Both images were viewed and are not black (the parity
    scene shows the lit room with the photon-map GI, solar0 a lit sphere on a floor).
- **Registration (T011):** `tests/visual/CMakeLists.txt` gets `photon-parity`, `photon-parity-slo`
  (threshold 6, `-t:1`), `solar0`, `solar0-slo` (default threshold), the two
  `VisualSetup_photon-parity{,-slo}-clean` fixtures and their `FIXTURES_REQUIRED`, with a comment citing
  R4. `tests/shading_parity/CMakeLists.txt` gets `ShadingParity_IlluminationHook`, only if
  `OPENRENDER_HAVE_LLVM`. Its `ENVIRONMENT` points `SHADERS` at `${CMAKE_BINARY_DIR}/shaders` and
  sets `DISPLAYS`, so that the default surface and light load at `RiWorldBegin` without noise.
  `src/libshader/tests/CMakeLists.txt` gets `LibShader_IlluminationOperands` inside the existing
  `if(OPENRENDER_HAVE_LLVM)` block.
- **Hook test (T012)** `tests/shading_parity/test_illumination_hook.cpp`. A probe context
  (`CHookProbeContext`, empty `renderingLoop()`, four logging hooks) runs `illuminate(P)`,
  `illuminate(P, N, a)`, `solar(D, a)` and `solar()` as `.rslo` and `.slo` through `execute()`, with the
  hook on (all points active, all inactive), off, and toggled on one loaded `.slo`.
- **Emitter test (T013)** `src/libshader/tests/test_illumination_emitter.cpp`: hand-built `IRModule`s
  through `emitLLVMBitcode()`.

Command: `ctest --test-dir build-024 -R 'Visual_photon-parity|Visual_solar0|ShadingParity_IlluminationHook|LibShader_IlluminationOperands|VisualSetup_photon' --output-on-failure`
(8 tests; 4 pass and 4 fail, every failure by assertion, exit code or diff, none by a harness error):

| Test | Result | Evidence |
|---|---|---|
| `VisualSetup_photon-parity-clean`, `-slo-clean` | pass | |
| `Visual_photon-parity` | **pass** (control) | diff 0.00 |
| `Visual_solar0` | **pass** (control) | |
| `Visual_photon-parity-slo` | **fail** | `orender failed with exit code 11`, no map written (the R4 crash in `jitIlluminateBegin`) |
| `Visual_solar0-slo` | **fail** | `MaxBlockAvgDiff: 201.59 (threshold: 20)  FailBlocks: 175` (R1 measured 201.62) |
| `ShadingParity_IlluminationHook` | **fail**, 75 passed and 15 failed | see below |
| `LibShader_IlluminationOperands` | **fail**, 5 passed and 7 failed | see below |

`ShadingParity_IlluminationHook` (no `.rslo` row fails; every failure is a `.slo` row):
- hook on, for all four fixtures, both all-active and all-inactive: `.slo calls: 0 call(s)`, against the
  contract table's two calls (8 failures);
- hook on, all points active, `illuminate1`, `illuminate3` and `solar2`: the JIT modified tags/L
  (`numActive=4 numPassive=0`), i.e. it ran its culling instead of delegating; and all-inactive
  `solar0`: `numActive=0 numPassive=4`, because with no begin call the JIT's `jitSolarEnd` still runs
  its tag-restore loop on points that were never incremented (4 failures in all);
- hook off, `solar0`: state differs from the interpreter, with `.rslo` `L == Ps` and `.slo`
  `L = (777 777 777)` (the sentinel, `L` never written), plus `L == Ps` failing for `.slo` and the
  toggle comparison failing (3 failures);
- hook off for `illuminate1`, `illuminate3` and `solar2` already matches across backends, and the
  control rows (all `.rslo` rows) pass.

`LibShader_IlluminationOperands`: the positive control (`illuminate P` compiles, writes a `.slo`, calls
`op_illuminate_begin`) and the label-only `solar` compile pass; the five failures are the two the
spec expects:
- label-only `solar`: the bitcode has no `op_solar0_begin`;
- both negative cases (`illuminate __no_such_var__`, `solar __no_such_var__ 0`): the emit returns
  true, writes a `.slo`, and logs nothing.

### Implementation (T014 to T019) and verification (T020 to T022), 2026-10-16

- **T014:** `CShadingContext::rendererIlluminationHook()` (`SVC && SVC->hasIlluminationHook()`) replaces
  `rendererHiderFlags()` in `shading.h`/`shading.cpp`.
- **T015:** the six illuminate/solar macros in `shaderOpcodes.h` test `this->rendererIlluminationHook()`.
  The file was clang-format clean before, so it was re-formatted; that re-aligns the backslashes of the
  edited macros (94 changed lines, 6 of them real).
- **T016:** `jitIlluminateBegin`, `jitIlluminate3Begin`, `jitSolarBegin`, `jitIlluminateEnd` and
  `jitSolarEnd` check the hook first and return after calling it, without reading `Ns`/`costheta` and
  without touching the tags, the counts or `L`. No null guard on `costheta`.
- **T017:** `jitSolar0Begin` (`shading.h`/`shading.cpp`) and the C-linkage `op_solar0_begin`
  (`rslOps.h`/`rslOps.cpp`).
- **T018:** `llvmEmitter.cpp`: a `solar` whose only operand is its label emits `op_solar0_begin`;
  `illuminate` with 2 or 4 operands and `solar` with 1 or 3 operands emit their begin call, and any
  other shape, or an operand that does not resolve, fails the emit with a diagnostic naming the
  mnemonic. The build compiles all 107 shipped shaders with `--jit` and the new error did not fire on
  any of them.
- **T019:** deleted the six `rsl_illuminate_*`/`rsl_solar_*` wrappers from `rslBuiltins.{h,cpp}`
  (320 lines); removed `hiderFlags()` from `rendererServices.h`, `rendererServicesImpl.h` and
  `recording_services.h`. Also removed the four `callIlluminateBegin/End` and `callSolarBegin/End`
  helpers in `shading.h`: their only callers were the deleted wrappers, and SC-007 says no hook-aware
  illumination entry point may be left without a caller (this goes one step beyond T019's list).
- **T020 results** (`nice -n 19`, `-j4`, build exit 0):
  - T-3a command, 8 tests: **8/8 pass**, including `Visual_photon-parity-slo`,
    `Visual_solar0-slo`, `ShadingParity_IlluminationHook` and `LibShader_IlluminationOperands`
    (each red before).
  - **`Visual_photon-parity-slo`: MaxBlockAvgDiff 0.00** against the interpreter reference
    (threshold 6). **Its map `photon-parity-slo.gpm` is 2 009 546 bytes**, equal to the interpreter's
    map at `-t:1`, so SC-005 holds.
  - `-L "shading_parity|libshader"`: 19/19. `-L visual`: 348/348.
  - **Full suite: 607/607**, 113 s. 607 = the 599 of Phase 2 plus 8 new tests: 4 visual scenes, 2
    `VisualSetup_photon-parity*-clean` fixtures, `ShadingParity_IlluminationHook` and
    `LibShader_IlluminationOperands`. `-L determinism`: 6/6. `archcheck`: PASS, violations 0, stale 0;
    Shading refs 9, distinct 5, TOTAL refs 749 (unchanged; this phase adds no `CRenderer::` reference).
  - A first full run gave 605/607. `Parity_transparency-matte-aov` (known-intermittent) failed once and
    passed on re-run in isolation. `ShaderCompilerImmutability` failed because the two new shaders had
    no reference `.rslo`: `tests/shaders/references/photonparity_view.rslo` and `solar0_light.rslo`
    were added (copied from the build's scratch output, the compiler's unchanged interpreter path), the
    test then passed 105/105 with 0 missing, and the later full runs were 607/607 three times.
    `Visual_sphere-shader-parameter-reyes` did not fail in any of these runs.
  - **SC-007 greps:**
    - `grep -rn 'rsl_illuminate\|rsl_solar' src`: nothing.
    - `grep -rn 'HIDER_ILLUMINATIONHOOK\|hiderFlags()\|rendererHiderFlags\|callIlluminate\|callSolar' src/libshader`:
      nothing.
    - `grep -n 'hiderFlags()' src/ri/render/rendererServicesImpl.h`: nothing. (The task's own pattern
      also listed `HIDER_ILLUMINATIONHOOK` for that file, which matches the one-line
      `hasIlluminationHook()` implementation there, the correct place for it; tasks.md T020 now says
      so, as quickstart §1.3 already did.)
- **T021:** `tests/visual/parity-thresholds.md` has a "Backend parity (spec 024)" section.
- **T022:** `DEVNOTES_DETAILS/BUGS.md`: a spec 024 data point added to the Open `CPhotonHider` SIGSEGV
  entry.

## Phase 4: US2, the shading library reaches the renderer only through its services

### Tests first (T023 to T025), paused 2026-10-16 before TEST-APPROVAL 4a

State when the work was paused (the machine was handed to another job):

- **T023 done.** `tests/architecture/allowlist.txt` lost the 10 DB-001 lines named in the task and its
  section comment now says (8). `tests/architecture/metrics-baseline.txt`: Shading `0 0`, TOTAL `740 117`;
  the Geometry (76 to 78) and Hiders (581 to 586) rows were raised to the measured values too, so that the
  rows add up to the TOTAL, with the reason stated in the file (#51's +7, never recorded; this spec's -9).
  Red, as expected: `ctest -L architecture` reports `RESULT FAIL violations=10 stale=0`, with exactly the
  10 `VIOLATION` lines (4 `include` of `renderer.h` from `rslo_generated.cpp`, `executeMisc.cpp`,
  `shader.cpp`, `shaderPl.cpp`; 6 `link` lines: `retrieveVariable` and `getDSO` from
  `rslo_generated.cpp`, `getGlobalID` from `shader.cpp`, `getStepFilter`, `retrieveVariable` and
  `getFilter` from `shaderPl.cpp`). It also warns, as the sources are unchanged, `Shading refs 9 > baseline 0`,
  `Shading distinct 5 > baseline 0` and `TOTAL refs 749 > baseline 740`.
- **T024 done, red.** Case D is in `test_services_routing.cpp`, and the test's `ENVIRONMENT` now points
  `SHADERS` at `${CMAKE_BINARY_DIR}/shaders` and sets `DISPLAYS`. `ctest -R ShadingServices_Routing
  --output-on-failure` on the unchanged tree (the fixture compiled and loaded; Cases A, B and C still pass):

  ```
  FAIL: wrapper.numRetrieveVariable >= 1  (test_services_routing.cpp:110)
  FAIL: wrapper.numGetGlobalID >= 2       (test_services_routing.cpp:111)
  Case D: a light loaded before RiWorldBegin() resolves through the installed services
    retrieveVariable x0, getGlobalID x0
  Results: 10 passed, 2 failed
  ```

  Both counts are 0, because `rslo.y` and `shader.cpp` still call `CRenderer::` directly.
- **T025 done, red.** `test_service_lookup.cpp` and its `EXCLUDE_FROM_ALL` registration are in place.
  `ctest -R ShadingServices_Lookup` reports "Not Run" (executable not built), and
  `cmake --build build-024 --target test_shading_service_lookup` fails to compile:

  ```
  test_service_lookup.cpp:19:10: fatal error: 'libshader/shading/dsoHandle.h' file not found
  ```

  `dsoHandle.h` is the first include the compiler reaches; `serviceLookup.h` is missing too and is the
  next one it would stop on. The test also uses `CStubServices::dso()`, which T026 adds.
