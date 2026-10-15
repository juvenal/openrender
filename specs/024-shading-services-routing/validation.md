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
