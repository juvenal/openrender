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

### Implementation (T026 to T031) and verification (T032), 2026-10-16

- **T026:** `class CDSO` moved verbatim from `renderer.h` to `src/libshader/shading/dsoHandle.h` (includes
  `ri/render/shadeop.h` for the `dso*Function` typedefs); `renderer.h` includes it; `execute.cpp` includes it
  explicitly. `CRendererServices::getDSO` added (`rendererServices.h`, forward declaration of `CDSO`), with
  `CRendererServicesImpl::getDSO` forwarding to `CRenderer::getDSO`. In `recording_services.h` the recording
  double counts `getDSO` and the stub returns the static `CStubServices::dso()` sentinel.
- **T027:** `src/libshader/shading/serviceLookup.{h,cpp}` (namespace `libshader`: `lookupDSO`, `lookupFilter`,
  `lookupStepFilter`, `lookupGlobalID`, `lookupVariable`), added to the library's `CMakeLists.txt`. With no
  services they return `nullptr`, `RiGaussianFilter`, `RiGaussianStepFilter`, `0` and `nullptr`, with no
  diagnostic yet (Phase 5). `EXCLUDE_FROM_ALL` removed from `test_shading_service_lookup`.
- **T028 to T031:** `rslo.y` (two `lookupVariable`, two `lookupDSO`), `shader.cpp` (`lookupGlobalID`) and
  `shaderPl.cpp` (two `lookupFilter`, one `lookupStepFilter`, one `lookupVariable`) go through the helpers;
  `#include "renderer.h"` is gone from `rslo.y`, `shader.cpp`, `shaderPl.cpp` and `executeMisc.cpp`. The only
  header `rslo.y` then needed was `common/memory.h` for `globalArena`.
- **T032 results** (`nice -n 19`, `-j4`):
  - Build: exit 0.
  - `ShadingServices_Routing` (Cases A, B, C and D) and `ShadingServices_Lookup`: both pass.
  - `ctest -L architecture`: 21/21. `archcheck`: `PASS violations=0 stale=0 badid=0 failing-cycles=0`, no `STALE`.
    Metric: **Shading refs 0, distinct 0**; **TOTAL refs 740 (baseline 740)**; TOTAL distinct **113** against the
    baseline 117, reported as `NOTE metric TOTAL distinct 113 < baseline 117`, so the baseline was lowered to 113
    in `metrics-baseline.txt` with the reason stated there.
  - **macOS import list (SC-001): 7 entries**, saved as `tools/libshader-dynlookup-after-macos.txt`: `CGatherBundle`
    ctor, `CPhotonMap::lookup`, `CShadingContext::trace` and `traceEx`, `CTexture3d::resolve`, and the vtables of
    `CTraceBundle` and `CTransmissionBundle`. The five `CRenderer::` symbols are gone (12 before).
  - `grep -rn 'include "renderer.h"' src/libshader`: nothing (SC-002).
  - `-R 'Visual_killeroo|Imager'`: 9/9. `-L determinism`: 6/6.
  - **Full suite: 608/608** (607 plus `ShadingServices_Lookup`, which is now built), 109 s.
- **Disk full during the first full run.** The first full run was 605/608: the disk had only about 210 MiB free,
  so two tests failed with "No space left on device" / "Could not create temporary file"
  (`ImageInput_TiffBakeByteIdentical`, `Visual_teapot-blue_marble-reyes-slo`), and `Visual_camera-dof-reyes`
  reported one block at 20.28 against threshold 20 (a stochastic DOF scene, run while the disk was full). About
  1.2 GB of `/tmp/shading_parity_*` and `/tmp/shading_services_routing_*` directories, left behind by the
  `shading_parity` tests' `mkdtemp` calls (including the two new tests), were deleted, after which the full suite
  passed 608/608 and `Visual_camera-dof-reyes` passed 10 of 10 repeats in isolation. These tests never remove
  their temp directories; that is pre-existing, and the two new tests follow the same convention.

## Phase 5: US3, shaders load and bind with or without a renderer

### Tests first (T033), 2026-10-16

- `tests/shading_parity/test_no_services.cpp` (test `ShadingServices_NoServices`, label `shading_parity`,
  `EXCLUDE_FROM_ALL` until T034). It runs in its own process, with no `RiBegin()` and no `CShadingContext`
  (#53), under a counting `CDiagnosticSink`:
  - each of the five helpers is called twice and must return its not-found value every time
    (`nullptr`, `RiGaussianFilter`, `RiGaussianStepFilter`, `0`, `nullptr`);
  - exactly 5 diagnostics must be reported, all `CODE_NOTSTARTED` at warning severity (one per kind);
  - `libshader::parseCategories("key,fill", out)` must return 0 with `out[0] == 0` (`out` is pre-filled with
    -1), and must add no further warning.
- Red: `cmake --build build-024 --target test_shading_no_services` fails to compile, and it is the only
  error the compiler reports:

  ```
  test_no_services.cpp:78:31: error: no member named 'parseCategories' in namespace 'libshader'
  ```

  `ctest -R ShadingServices_NoServices` reports "Not Run" (executable not built). The "warning count is 0"
  half of the red cannot be shown separately, because the file does not compile until `parseCategories`
  exists; with the T034 helpers in place the count assertions are the ones that decide green.

### Implementation (T034, T035) and verification (T036), 2026-10-16

- **T034:** `serviceLookup.cpp`/`.h`: one `std::atomic<bool>` per kind; the first lookup of a kind made with no
  services reports `warning(CODE_NOTSTARTED, "renderer services are not installed: %s lookup of \"%s\" returns
  not found\n", ...)`, later ones are silent. `libshader::parseCategories(const char *, int *)` tokenizes on
  `",\t "`, appends only non-zero IDs from `lookupGlobalID`, terminates the list with 0 and returns the count.
  `CShaderInstance::createCategories()` in `shader.cpp` uses it, so 0 is never stored before the terminator (real
  IDs start at 1: `numKnownGlobalIds = 1` in `initDeclarations()`); the file-local `token()` helper had no other
  caller and was removed. `EXCLUDE_FROM_ALL` removed from `test_shading_no_services`.
- **T035:** `RSLShading.h` and `RSLShadingState.h` no longer promise a `CRenderer::*` fallback; they describe the
  installed renderer services and the no-services behaviour (not found, one warning per kind, an unresolved
  category skipped).
- **T036 results** (`nice -n 19`, `-j4`, build exit 0, no first-party warnings):
  - `ShadingServices_NoServices`: passes (16 checks). Its run reports exactly the five warnings, one per kind,
    each `code 11` (`CODE_NOTSTARTED`) at severity 1 (warning).
  - `ShadingServices_Routing` and `ShadingServices_Lookup`: pass.
  - `-L shading_parity`: 11/11.
  - **Full suite: 609/609** (608 plus `ShadingServices_NoServices`), 102 s.

## Phase 6: Polish (documentation tasks), 2026-10-16

- **T037:** `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`. DB-001 (still Open): the 7 remaining imports in Evidence, the
  8 remaining edges and the gone `renderer.h` includes under "Measured by the architecture check", a spec 024
  History item (12 → 7 imports, 18 → 8 edges), and the spec 024 items dropped from Direction. DB-012 moved to
  Resolved (resolved by spec 024, 2026-10-16) with the fix summarised, and a note that its spec 026 and 029
  couplings are not lost (026 now sits under DB-018's Direction, 029 is tracked under DB-005). DB-018: the
  explicit ray type / hit mode in `ShaderGlobals` (spec 026, #32) added to Direction.
- **T038:** `CLAUDE.md`. The shader count line says 107 `.rslo` (+ 107 `.slo`), the measured number: the task
  predicted 106, but master already had 105 shaders before this spec's two, and the stale "138-object rebuild"
  next to it is now 214 (107 x 2; the "~2s" timing was not re-measured). New Known-gotchas item 23 (services
  registered at `RiBegin` and deregistered at `RiEnd`; Shading reaches the renderer only through
  `serviceLookup`/`CRendererServices`; the one `rendererIlluminationHook()` query; the photon hider never sets
  `costheta`). The Testing section lists `ctest -L shading_parity`. Active Technologies and Recent Changes were
  not touched, as T038 does not ask for them.
- **T039:** `DEVNOTES.md` has a status-table row for spec 024 (worded "pending Linux validation");
  `DEVNOTES_DETAILS/OSHADER_UPDATES.md` has a section on `op_solar0_begin`, the hard emitter error and the
  removed `rsl_*` wrappers.
- **Stopped before T040** (a [RUN] build in `build-024-nojit`); T041 (the Linux package and its sha256 manifest,
  which has to list the final changed files) and T042 (the final macOS pass) also come after it.

### T040: the build without the JIT, 2026-10-16

`nice -n 19 cmake -B build-024-nojit -S . -DOPENRENDER_ENABLE_JIT=OFF` (configure log: "JIT shader path disabled
by OPENRENDER_ENABLE_JIT=OFF -- shaders run interpreted"), `nice -n 19 cmake --build build-024-nojit --config
Release -j4` (exit 0, 1 min 30 s, no first-party warnings), then
`nice -n 19 ctest --test-dir build-024-nojit -L "architecture|shading_parity|visual" -j4 --output-on-failure`.

- **378 tests selected: 319 passed, 58 reported disabled ("Not Run (Disabled)"), 1 failed.**
- The 58 disabled are the `-slo` scenes (53, among them `Visual_photon-parity-slo` and `Visual_solar0-slo`) and the
  5 JIT-only `shading_parity` tests, as the configure log says.
- **`ShadingParity_IlluminationHook` and `LibShader_IlluminationOperands` are not registered** (`ctest -N -R
  'IlluminationHook|IlluminationOperands'` lists 0 tests), because they sit inside `if(OPENRENDER_HAVE_LLVM)`.
- Passing: the architecture label, `ShadingServices_Routing`, `_Lookup` and `_NoServices`, `Visual_photon-parity`
  and `Visual_solar0` (the interpreter scenes, which need no JIT).
- **The one failure, `Visual_camera-dof-reyes`: MaxBlockAvgDiff 20.38 against threshold 20, 1 block** (block
  (48,32), channel G). This is the same single block as the 20.28 seen once during Phase 4's disk-full run. It is
  a stochastic depth-of-field scene: 10 isolated runs of it gave 15.34 to 18.48 in the no-JIT build and 15.28 to
  18.52 in `build-024` (JIT on), i.e. the same noise band with or without the JIT, and a run under `-j4` load
  lands above 20 now and then. It is not one of the four known-intermittent tests listed in this spec, it is not
  related to Shading's services or the hook (the scene has no light shader constructs and no photon pass), and no
  threshold was changed. Worth a note to the maintainer: its threshold of 20 sits only about 2 above its normal
  worst case.
- `build-024-nojit` was deleted afterwards.
- **Shader rebuild timing** (for CLAUDE.md): in `build-024`, `touch shaders/includes/displacement.slh` then
  `cmake --build build-024 --target openrender_shaders -j4` recompiled all 107 `.rslo` and 107 `.slo` (214
  objects) in 1.36 s; two further runs recompiled only part of them (20 and 87 each, in 0.5 s and 1.1 s), because
  repeated `touch` calls inside the same second race with the dependency check. CLAUDE.md now says ~1.4 s at
  `-j4`. The comment in `shaders/CMakeLists.txt` still quotes "69 shaders / 138 objects / ~2s at -j8"; it was
  not part of this task and was left alone.

## T041: Linux validation package (prepared 2026-10-16; to be run by the coordinator)

Not run yet. Base: master `c62334d`. `git diff --name-status master...HEAD` shows **77 changed files (44 added, 33
modified) and none deleted or renamed**, so nothing has to be removed by hand on Linux. The manifest also covers the
comment fix in `shaders/CMakeLists.txt` (staged, not yet committed when the manifest was written); that makes 78 entries.

Linux checkout: `juvenal@ubuntu-2404-desktop.v2-labs.io:/home/juvenal/Projects/Development/CLI/openrender`
(`L` below). Ubuntu 24.04, GCC 13, LLVM 18. `-Werror` is not a flag to pass: `CMakeLists.txt` puts it on
`openrender_common_flags`, which every target links.

### 1. Before the sync (on the Linux host)

```bash
cd /home/juvenal/Projects/Development/CLI/openrender
git status --short | head        # tracked files with local edits would be overwritten by the sync below
git log -1 --oneline             # expect master c62334d or a descendant that does not touch Shading, the JIT or tests/
```

### 2. Sync (on the Mac, from the worktree root)

Only `git ls-files` output is sent: no `--delete`, and `.git`, `build*/` and untracked files are never included.
`--checksum` makes rsync compare content, because the two checkouts' mtimes differ. Run the dry run first.

```bash
cd /Volumes/Projects/Development/CLI/openrender-worktrees/024-shading-services-routing
L=juvenal@ubuntu-2404-desktop.v2-labs.io:/home/juvenal/Projects/Development/CLI/openrender/

# dry run: lists what would be sent
git ls-files -z | rsync -a --checksum --from0 --files-from=- --dry-run --itemize-changes ./ "$L"

# the real sync
git ls-files -z | rsync -a --checksum --from0 --files-from=- --itemize-changes ./ "$L"
```

Deleted by this branch relative to master: **none**.

### 3. Verify the sync (on the Linux host)

```bash
cd /home/juvenal/Projects/Development/CLI/openrender
sha256sum -c specs/024-shading-services-routing/tools/024-linux-manifest.sha256 | grep -v ': OK$'   # expect no output
```

The manifest is `tools/024-linux-manifest.sha256`: `sha256sum` lines (`<hash>  <path>`, paths relative to the repo
root) for every file in `git diff --name-only master...HEAD`, plus `shaders/CMakeLists.txt`, written after the last
edit of this file. It does not list itself. If `validation.md` or `tasks.md` is edited after this point they will
show as mismatches until the manifest is regenerated:

```bash
# on the Mac, worktree root
( git diff --name-only master...HEAD; git diff --cached --name-only ) | sort -u \
  | grep -v '^specs/024-shading-services-routing/tools/024-linux-manifest.sha256$' \
  | xargs shasum -a 256 > specs/024-shading-services-routing/tools/024-linux-manifest.sha256
```

### 4. Build (on the Linux host): a fresh `build-024`

```bash
cd /home/juvenal/Projects/Development/CLI/openrender
rm -rf build-024
cmake -B build-024 -S . -DCMAKE_BUILD_TYPE=Release -DINSTALL_SELFCONTAINED=OFF \
      -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13
nice -n 19 cmake --build build-024 -j4
```

Record any warning that `-Werror` turns into an error (the spec's own files are the suspects: `serviceLookup.cpp`,
`test_illumination_hook.cpp`, `test_no_services.cpp`, `llvmEmitter.cpp`; Clang misses some GCC 13 array-bounds and
sign-compare cases).

### 5. Tests (on the Linux host), one at a time

```bash
nice -n 19 ctest --test-dir build-024 -L architecture -j4 --output-on-failure
nice -n 19 ctest --test-dir build-024 -L shading_parity -j4 --output-on-failure
nice -n 19 ctest --test-dir build-024 -L libshader -j4 --output-on-failure
nice -n 19 ctest --test-dir build-024 -R 'Visual_photon-parity|Visual_solar0' --output-on-failure
nice -n 19 ctest --test-dir build-024 -j4 --output-on-failure
```

Expected: the full suite is 609 tests on this branch. The only failures allowed are the known Linux ones from spec
023's T060 (`specs/023-common-infrastructure-floor/validation.md`); `LibShader_McompConsistency` (#44) still fails,
now on `_ZTV19CTransmissionBundle`. Re-run any other failure alone before reporting it.

Record from this run:
- **`Visual_photon-parity-slo` MaxBlockAvgDiff on Linux.** The JIT-against-interpreter difference adds to the 2.91
  cross-platform difference there, so if it is above 6, stop and report. Never edit the threshold.
  `ctest -R 'Visual_photon-parity-slo' -V | grep MaxBlockAvgDiff`
- **The map size** (SC-005): `wc -c build-024/tests/visual/photon-parity-slo/photon-parity-slo.gpm`. It must exist and
  be non-zero; R4 measured 2 009 546 bytes on Linux.
- **The `ShadingServices_*` and `ShadingParity_IlluminationHook` results**, and the ctest totals.

### 6. SC-001 on Linux (import list)

```bash
cd /home/juvenal/Projects/Development/CLI/openrender
L=build-024/src/libshader/shading/libshader_shading.so.1
nm -D --undefined-only "$L" | awk '$1=="U"{print $2}' | sed 's/@.*//' | sort -u > /tmp/024-u.txt
SHADERS="$PWD/build-024/shaders" ORENDERHOME="$PWD/build-024/tests/visual/_home" \
DISPLAYS="$PWD/build-024/src/display/file" \
LD_BIND_NOW=1 LD_DEBUG=bindings build-024/src/orender/orender examples/rib/camera-dof.rib 2>&1 \
  | grep -E 'binding file [^ ]*libshader_shading\.so\.1 \[0\] to [^ ]*libRIBRender\.so\.1' \
  | sed 's/.*normal symbol `//; s/'"'"'.*//' | sort -u > /tmp/024-b.txt
comm -12 /tmp/024-u.txt /tmp/024-b.txt | c++filt | tee /tmp/024-libshader-dynlookup-after-linux.txt
wc -l < /tmp/024-libshader-dynlookup-after-linux.txt     # expect exactly 7
```

The 7 are the Hiders and Texturing set (`CGatherBundle`, `CPhotonMap::lookup`, `CShadingContext::trace`/`traceEx`,
`CTexture3d::resolve`, the vtables of `CTraceBundle` and `CTransmissionBundle`). Copy
`/tmp/024-libshader-dynlookup-after-linux.txt` back as
`specs/024-shading-services-routing/tools/024-libshader-dynlookup-after-linux.txt`.

Linux green is required before the maintainer merges; this task itself does not depend on the merge.

## T042: final macOS pass, 2026-10-16

On the branch as committed, plus the staged comment fix in `shaders/CMakeLists.txt` (`nice -n 19`, `-j4`, build
exit 0 with nothing to compile but the shader step):

- **`ctest -L architecture`: 21/21.** `archcheck`: `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0
  notbuilt=0`; Shading refs 0 (baseline 0), TOTAL refs 740 (baseline 740), TOTAL distinct 113 (baseline 113), so no
  `WARNING` and no `NOTE`.
- **`git diff df77ded -- tests/architecture/allowlist.txt`:** exactly the 10 DB-001 deletions (4 `renderer.h`
  includes, 6 `CRenderer::` link lines) plus the section comment `(18)` to `(8)`. Nothing is added.
- **`metrics-baseline.txt` against `df77ded`:** Shading `9 5` to `0 0`, TOTAL `742 117` to `740 113`, both
  decreases. The Geometry (76 to 78) and Hiders (581 to 586) rows went up to the values #51 added on master, with
  the reason in the file (maintainer-approved in Phase 4); the TOTAL is still below what it was.
- **`-L determinism`: 6/6** (SC-006).
- **Full suite: 608/609 on the first run, 609/609 counting the re-run.** `Parity_transparency-matte-aov`
  (known-intermittent) failed once in the full run and passed 5 of 5 alone. `Visual_camera-dof-reyes`, the other
  known flake, did not fail in this run.
- The manifest `tools/024-linux-manifest.sha256` was regenerated after the edits to this file and to `tasks.md`
  (both are manifest-covered; only these two entries changed) and verified with `shasum -a 256 -c`. T041 stays
  unticked: the Linux run is still to be recorded.

### T041 results: Linux, 2026-10-16 (run by the coordinator)

Ubuntu 24.04, GCC 13.3, a fresh `build-024` (Release, `-DINSTALL_SELFCONTAINED=OFF`, `-Werror`), after an rsync of
99 files; all 78 manifest entries were OK on Linux and a SHA-256 over all 2682 tracked files was identical on both
sides. The host has one CPU.

- **Build:** exit 0, no warnings, no errors.
- **`-L architecture`: 21/21.** **`-R 'Visual_photon-parity|Visual_solar0'`: 6/6.**
- **`Visual_photon-parity-slo` MaxBlockAvgDiff 2.91** (threshold 6), the same as the constant Linux-against-macOS
  difference of R4, so the JIT variant adds nothing on top of it. **Its map is 2 009 546 bytes**, identical to the
  interpreter's and to macOS (SC-005).
- **SC-001 on Linux: exactly 7** import bindings, saved as `tools/024-libshader-dynlookup-after-linux.txt`: `CPhotonMap::lookup`,
  `CTexture3d::resolve`, `CGatherBundle::CGatherBundle()`, `CShadingContext::trace`, `CShadingContext::traceEx` and the
  vtables of `CTraceBundle` and `CTransmissionBundle`. The five `CRenderer::` symbols are gone (12 before).
- **Full suite: 610 tests, 13 failed.** Sorted:
  - **The known Linux failures from spec 023's T060 (8):** `Parity_transparency-matte-aov`,
    `Visual_blobby-max-union-reyes`, `TextureTile_RenderByteIdentical`, the four `TextureTile_Cache*` tests, and
    `LibShader_McompConsistency` (#44, still failing on `_ZTV19CTransmissionBundle`).
  - **This spec's own (2): `ShadingServices_Lookup` and `ShadingServices_NoServices`**, which failed at load time with
    `symbol lookup error: libshader_shading.so.1: undefined symbol: _ZTV19CTransmissionBundle`. Cause, confirmed with
    `readelf -d`: Ubuntu's GCC links with `--as-needed`; these two tests reference no `libRIBRender` symbol, so
    `libRIBRender` was dropped from their NEEDED list (only `libshader_shading` and `libRIBCommon` remained), and
    `libshader_shading`'s 7 remaining DB-001 imports could not resolve. `test_shading_services_routing` keeps
    `libRIBRender` because it calls `RiBegin`. This is the same class of problem as #44. **Fixed** (see below).
  - **Not this spec's (3), master/#51 Linux issues:** `Visual_sphere-occlusion-reyes` and `Visual_sphere-occlusion-reyes-slo`
    (24.58 against 20, the same in 5 of 5 runs, so deterministic) and
    `CoverageAnalytic_alpha-sides2-tjunction-r4_reyes` (1 pixel off). They come from #51 (`1c06579` re-rendered the
    sphere-occlusion reference and added the T-junction tests), which was never validated on Linux; this branch changes
    no hider or geometry code, and the interpreter scene and its `-slo` variant fail together (both are compared to the
    same re-rendered reference).

**The fix (macOS-verified, to be re-run on Linux):** `tests/shading_parity/CMakeLists.txt` gives
`test_shading_service_lookup` and `test_shading_no_services`
`target_link_options(... PRIVATE $<$<PLATFORM_ID:Linux>:LINKER:--no-as-needed>)`, commented with the reason and #44, so
`libRIBRender` stays NEEDED. On macOS the generator expression is empty. Locally: configure and a full build exit 0,
`-L shading_parity` 11/11 and `-L architecture` 21/21 pass.

**Linux re-run after the link fix (coordinator, 2026-10-16):**
- The re-sync sent `validation.md`, the new dynlookup file, the manifest and `tests/shading_parity/CMakeLists.txt`;
  `sha256sum -c` of the manifest gave 0 mismatches.
- `cmake --build build-024 --target test_shading_service_lookup test_shading_no_services` built both with no warnings.
- `readelf -d` shows `libRIBRender.so.1`, `libshader_shading.so.1` and `libRIBCommon.so.1` in both executables'
  NEEDED lists.
- **`ctest -L shading_parity` on Linux: 11/11 passed**, including `ShadingServices_Lookup` and
  `ShadingServices_NoServices`.

**Linux final: 610 tests; 11 failures, all outside this spec.** The 8 known Linux failures of spec 023's T060 plus
the 3 master/#51 failures listed above; no failure of this spec's own (the two `ShadingServices_*` failures are
fixed). The 3 are `Visual_sphere-occlusion-reyes`, `Visual_sphere-occlusion-reyes-slo` and
`CoverageAnalytic_alpha-sides2-tjunction-r4_reyes`, to be recorded as master/#51 Linux issues. T041 is complete.
