# Validation log: spec 023

`$S` is the session scratchpad:
`/private/tmp/claude-501/-Volumes-Projects-Development-CLI-openrender/c6901561-89e2-4e24-999d-cbd0f67d87e7/scratchpad`.

## Phase 1: baselines (before any source edit)

### T001: the baseline is pristine

- Branch `023-common-infrastructure-floor`, `HEAD` `9b8f89f`. Specs only on top of master
  `010ce04`.
- `git diff --stat 010ce04 -- src tests CMake CMakeLists.txt` prints nothing.
- `build/` configuration:
  - `CMAKE_BUILD_TYPE=Release`
  - `INSTALL_SELFCONTAINED=ON`
  - `CMAKE_INSTALL_PREFIX=/usr/local`
  - vcpkg manifest mode, with
    `VCPKG_INSTALLED_DIR=/Users/juvenal/.cache/vcpkg-installed/openrender`
- `nice -n 19 cmake --build build --config Release -j4` is a no-op.

### T002: frozen baseline install

- `DESTDIR=$S/023-base nice -n 19 cmake --install build` exited 0. The log is
  `$S/023-base-install.log`.
- `/usr/local` was snapshotted before and after: no difference.
- Layout under `$S/023-base/usr/local`:

  | Directory | Contents |
  |---|---|
  | `lib/` | `libRIB{Render,Vector,Out}.1.dylib` and `.a`; `libRIBCommon.a` (static only); `libshader_{compiler,runtime,shading}.1.dylib`; `cmake/openRender/` |
  | `bin/` | `orender`, `orender-fb`, `orender-wire`, `oshader`, `otexmake`, `sloinfo`, `rsloinfo` (a symlink to `sloinfo`) |
  | `displays/` | `file.dsply`, `framebuffer.dsply`, `openexr.dsply`, `rgbe.dsply` |
  | `shaders/` | `.sl`, `.rslo` and `.slo` |
  | `include/` | `ri.h`, `dsply.h`, `dlo.h`, `implicit.h`, `ptcapi.h`, `shadeop.h`, `rslo.h`, `file_base.h` |

- Install names are `@rpath/…`. Installed libraries and executables carry
  `LC_RPATH @loader_path/../lib`.
- The case-only compatibility aliases (`libribOut`, `libribVector`) are skipped on this
  case-insensitive volume, as designed.

### T003–T004a: compatibility consumers (SC-006 baseline)

The tools live in `tools/compat-consumer/`:

| File | Role |
|---|---|
| `consumer.cpp` | The consumer program |
| `common.sh` | Shared helpers |
| `build.sh` | Builds the three consumers |
| `run.sh` | Runs them |
| `compare.sh` | Compares outputs between two tags |
| `render-installed.sh` | Renders with the installed `orender` |
| `build-package.sh` | Builds the CMake package consumer |

1. **In-place root**: `cp -a $S/023-base $S/023-inplace`. `$S/023-base` stays untouched as the
   performance A side.
2. **Build**: `build.sh $S/023-inplace $S/023-compat` built `consumer-RIBRender`,
   `consumer-RIBVector` and `consumer-RIBOut`, each against exactly one baseline RI library, with
   `LC_RPATH` into `$S/023-inplace/usr/local/lib`. These binaries are kept **unchanged** for T051.
3. **Run**: `run.sh $S/023-inplace base` set no loader variables, and every run exited 0.
   - Resolution: `consumer-RIBRender` → `libRIBRender.1.0.0` + `libshader_shading.1.0.0`;
     `consumer-RIBVector` → `libRIBVector.1.0.0`; `consumer-RIBOut` → `libRIBOut.1.0.0`. All
     resolved inside `$S/023-inplace`.
   - Outputs: a 1481-byte TIFF (a shaded sphere, checked visually) and two 789-byte RIB files.
   - `libRIBVector`/`libRIBOut` warn `Unknown option: "shaderformat"` and
     `Unknown searchpath option: "geometry"`, read from the install's `.orenderrc`. This is
     existing behavior, unrelated to this spec.
4. **Installed render**: `render-installed.sh $S/023-base $S/023-render-base` ran the installed
   `orender` on `examples/rib/camera-dof.rib` and wrote `camera-dof.tif` (21611 bytes) through the
   installed `file.dsply`. Libraries resolved inside `$S/023-base`.
5. **Package consumer**: `build-package.sh $S/023-base $S/023-pkg-base` exited 0. `pkg-shared`
   (`openRender::RIBRender`) and `pkg-static` (`openRender::RIBRender_static`) both rendered a
   1481-byte TIFF, byte-identical to `consumer-RIBRender`'s output.
   - **Finding (existing, not this spec)**: the `Static` component fails to link without vcpkg's
     toolchain (`_lzma_stream_encoder` undefined in `libtiff.a`). The package config's plain
     `find_package(TIFF)` doesn't carry vcpkg's static transitive dependencies.
   - `build-package.sh` therefore configures with vcpkg's toolchain in classic mode
     (`VCPKG_MANIFEST_MODE=OFF`, the existing installed tree) when `VCPKG_ROOT` and
     `VCPKG_INSTALLED_DIR` are set.
   - This is worth a GitHub issue against the packaging (memory `project_install_packaging.md`).
     It is not filed: that is the maintainer's call.
6. **T004a determinism probe**: `run.sh $S/023-inplace base2`, then
   `compare.sh $S/023-compat base base2`.
   - The TIFF is **byte-identical**.
   - The RIB files differ **only** in the `##CreationDate` header line, which
     `libRIBOut`/`libRIBVector` stamp with the wall-clock time.
   - `compare.sh` encodes this rule: `cmp` for `.tif`, `diff` with `##CreationDate` excluded for
     `.rib`. The result was IDENTICAL for all three outputs. T051 uses the same command with tags
     `base new`.

### T005: shading-library "before" lists

- **macOS**:
  - `tools/libshader-undef-before-macos.txt` lists the 439 undefined symbol names (`nm -u`).
  - `tools/libshader-nm-m-before-macos.txt` is the same list with binding info (`nm -m`), taken
    from the pristine `build/`.
  - Exactly **29** are flat-namespace imports ("dynamically looked up"):

    | Group | Count | Symbols |
    |---|---|---|
    | US2 infrastructure (leaves in this spec) | 17 | `memoryInit`, `memoryTini`, `memoryNewPage`, `parseVariable`, `sampleHemisphere`, `sampleCosineHemisphere`, `info`, `error`, `warning`, `CRefCounter` ctor/dtor/typeinfo, `CFileResource` ctor/dtor/typeinfo, `CRenderer::globalMemory`, `stats` |
    | DB-001 remainder (specs 024/029) | 12 | `CPhotonMap::lookup`, `CTexture3d::resolve`, `CGatherBundle` ctor, `CShadingContext::trace`/`traceEx`, `CRenderer::{getGlobalID, getStepFilter, retrieveVariable, getDSO, getFilter}`, vtables for `CTraceBundle`/`CTransmissionBundle` |

- **Linux** (run by the maintainer on master's `build-022`):
  - `tools/023-libshader-undef-before-linux.txt` has 463 lines (`nm -D --undefined-only`). All 17
    US2 infrastructure symbols are present as `U`, matching macOS.
  - `tools/023-libshader-bindings-before-linux.txt` was **empty**, because the first command block
    was wrong. It grepped for `file=`, but glibc's `LD_DEBUG=bindings` lines read
    `binding file <obj> [0] to <lib> [0]: normal symbol \`<mangled>'`. Lazy binding also leaves
    out functions that are never called. The corrected command, for the maintainer to re-run:

  ```bash
  cd /home/juvenal/Projects/Development/CLI/openrender
  SHADERS="$PWD/build-022/shaders" ORENDERHOME="$PWD/openrender" \
  LD_BIND_NOW=1 LD_DEBUG=bindings build-022/src/orender/orender examples/rib/camera-dof.rib 2>&1 \
    | grep 'binding file .*libshader_shading' \
    | grep -E '_Z5error|_Z7warning|_Z4info|`stats|memory(Init|Tini|NewPage)|globalMemory|parseVariable|Hemisphere|CRefCounter|CFileResource' \
    > specs/023-common-infrastructure-floor/tools/023-libshader-bindings-before-linux.txt
  wc -l specs/023-common-infrastructure-floor/tools/023-libshader-bindings-before-linux.txt
  ```

  Expect about 17 lines (more if one symbol is bound from several relocations), every one ending
  `to …/libRIBRender.so.1 …`. After the feature, the same command must show them bound to
  `libRIBCommon.so.1`.

### T006: golden variable-declaration table (FR-005)

- `tests/unit/common/test_common_variables.cpp` and `tests/unit/common/CMakeLists.txt` are new.
  `tests/CMakeLists.txt` gains `add_subdirectory(unit/common)`.
- The test registers as `Common_VariableDeclarations`, with labels `common` and `unit`.
- The matrix covers:
  - 9 storage-class prefixes: none, `global`, `constant`, `uniform`, `varying`, `facevarying`,
    `vertex`, `global uniform`, `global varying`;
  - 13 type keywords;
  - 4 array forms: none, `[1]`, `[4]`, ` [ 3 ] `;
  - each with and without a declared name.

  That is 936 rows. The remaining 22 rows are 3 cases using the separate `name` argument, a
  62-character name (the longest the lexer's `assert` allows), and 18 invalid or unusual forms.
  **958 rows** in total.
- Every row records the return value and every `CVariable` field. Fields the parser never writes
  are pre-set to a sentinel (`-7`), so writes to them would show up.
- `tests/unit/common/variable_golden.txt` is the `--dump` output from the pristine baseline. Two
  dumps were byte-identical.
- The table records today's quirks verbatim, which is the point of a golden table. Examples:
  - a failed parse can still write the half-read name (`"bogus float"` → `name="bogus"`);
  - `"color[2.5]"` truncates to 2 items;
  - `"color[0]"` is accepted with 0 floats.
- Results:
  - `ctest -L common`: 1/1 pass;
  - `ctest -L "common|architecture"`: 22/22 pass;
  - full build clean (no warnings).
- Full suite (`nice -n 19 ctest --test-dir build -j4`): **559/559 pass** in 124 s. That is the 558 tests already on master plus `Common_VariableDeclarations`.

## Phase 2: Foundational (shared `libRIBCommon`)

### T007: `Common_StateOwnership`, red (TEST-APPROVAL 2a)

- New files:
  - `tests/unit/common/state_ownership.cmake`, a `cmake -P` script;
  - `tests/unit/common/owned-symbols.txt`.

  Registered in `tests/unit/common/CMakeLists.txt` with labels `common` and `unit`.
- **Images**: 17 scanned (resolved at generate time, because `tests/` is configured before `src/`):
  - `libRIB{Render,Vector,Out}`, `libshader_{shading,compiler,runtime}`;
  - the display modules `file`, `framebuffer`, `rgbe`, `openexr`;
  - `orender`, `oshader`, `otexmake`, `precomp`, `sloinfo`, `test_common_variables`;
  - plus the `openrendercommon` output.
- **Seed list**: `RiBezierBasis`, `RiGaussianFilter`, `RiCatmullRomFilter` (from
  `rslConstants.cpp`) and `_Z9osCPUTimev`.
- **Red output (expected)**:
  - each `rslConstants` symbol has **5 definers**: `libRIBVector`, `libRIBOut`,
    `libshader_shading`, `otexmake` and the archive;
  - `osCPUTime` has **11**: every shared library, all four display modules, `orender` and the
    archive.

  This is the per-image copy problem FR-008 targets: every image that embeds the static archive
  carries its own copy.
- Two script bugs were found and fixed while making it red for the right reason:
  - In `-P` script mode `APPLE` is undefined, so the script now uses `CMAKE_HOST_APPLE`.
  - CMake's `REGEX REPLACE "^_"` strips *every* leading underscore (it re-anchors after each
    match), so the script now removes exactly one with `SUBSTRING`.

### T008–T014: shared `libRIBCommon` (packaging only, no symbol moves)

**What changed**:

| Task | File(s) | Change |
|---|---|---|
| T008 | `src/common/CMakeLists.txt` | `openrendercommon_obj` (OBJECT, PIC) feeds **SHARED** `openrendercommon` (`libRIBCommon.1.dylib`, `@rpath` install name, `SOVERSION ${OPENRENDER_COMPAT_SOVERSION}`; `--no-undefined` on Linux) and **STATIC** `openrendercommon_static` (`libRIBCommon.a`) |
| T009/T010 | `src/ri/CMakeLists.txt` | `openrender_reexport_common()` gives `ribRender`, `ribVector` and `ribOut` `-reexport_library` on APPLE. `ribRender_static` keeps the **shared** common; `ribVector_static`/`ribOut_static` → `openrendercommon_static` |
| T011 | `src/libshader/{compiler,runtime}/CMakeLists.txt` | The shared libraries now `PUBLIC`-link `openrendercommon` (no embedded copy); their `_static` variants → `openrendercommon_static`. `libshader_shading` is unchanged (`PRIVATE openrendercommon`, now shared); a comment was added |
| T012 | `CMake/openRenderConfig.cmake.in`, root `CMakeLists.txt` | Comments updated. The `-force_load` block is unchanged |
| T013 | `src/preview/orender-wire-macos/CMakeLists.txt` | The app links `-lRIBCommon` (shared) instead of `libRIBCommon.a`, and bundles `libRIBCommon.1.dylib` plus a symlink in `Contents/Frameworks`. The self-contained bundling block needs no change: it copies only dependencies resolved *outside* the prefix |

**Verification (T014)**:
- **T009 spike**: no ld64 warning. `LC_REEXPORT_DYLIB @rpath/libRIBCommon.1.dylib` is present on `libRIB{Render,Vector,Out}.1.dylib`.
- **Linkage** (`otool -L`): `libRIBCommon` is linked by `orender`, `libshader_shading`,
  `libshader_compiler`, `file.dsply` and `orender-wire.app/Contents/MacOS/orender-wire`.
  `Contents/Frameworks` holds `libRIBCommon.1.dylib`, `libRIBCommon.dylib` and the `libRIBVector`
  pair.
- **Tests**:
  - `Common_StateOwnership` is **green**: all 4 seed symbols are defined once, in
    `libRIBCommon.1.dylib`, across 17 images.
  - `ctest -L "common|architecture"`: 23/23.
  - Full suite: **560/560 pass** (128 s).
- **Scratch install** (`DESTDIR=$S/023-p2`; `/usr/local` unchanged):
  - `lib/` holds `libRIBCommon.{1.0.0,1,}.dylib` and `libRIBCommon.a`.
  - `displays/file.dsply` links `@rpath/libRIBCommon.1.dylib` with `LC_RPATH @loader_path/../lib`.
  - Exported targets:
    - `openRender::RIBCommon` (SHARED) and `openRender::RIBCommon_static` (STATIC);
    - `RIBRender_static` → `openRender::RIBCommon`;
    - `RIBVector_static`/`RIBOut_static`/`shader_*_static` → `openRender::RIBCommon_static`;
    - `shader_compiler`/`shader_runtime` → `openRender::RIBCommon`.
  - `build-package.sh $S/023-p2`: `pkg-shared` and `pkg-static` both render.
    - `pkg-static` links `libshader_shading` and the shared `libRIBCommon`, and defines **none**
      of the seed symbols itself (one copy per process).
    - Its TIFF is byte-identical to the baseline `pkg-static.tif`.
  - `render-installed.sh $S/023-p2`: `camera-dof.tif` renders through the installed `file.dsply`.
    Its size differs from the baseline (21577 vs 21611 bytes); that is expected, because
    `camera-dof` is a multi-threaded DOF render and is not one of the byte-compared outputs.
- **Linker warnings**:
  - New: the deprecated `-undefined error`, now removed.
  - Already present before this spec: `orender-wire`'s "built for macOS 13.3, linked 13.0"
    (the Swift package targets 13.0), which now also names `libRIBCommon`; and "ignoring
    duplicate libraries" (Imath, LLVM).

## Phase 3: US3, diagnostics (tests first)

### T015–T018: red (TEST-APPROVAL 3a)

**New files**, all in `tests/unit/common/`, registered in its `CMakeLists.txt` as
`EXCLUDE_FROM_ALL` until T021:

| File | Test |
|---|---|
| `test_common_diagnostics.cpp`, `diagnostics_runner.cmake` | `Common_DiagnosticsNoContext_{error,warning,info,info_quiet,fatal}`. Links **only** `openrendercommon`, so reporting must need no renderer library |
| `test_common_crosslib.cpp` | `Common_DiagnosticsCrossLibrary`: case 0 (a capture sink survives two `RiBegin`/`RiEnd` sessions and gets reports between them), case 1 (the RI handler gets `RIE_BADTOKEN` with `RIE_ERROR`/`RIE_WARNING`/`RIE_INFO`), case 2 (`dlsym`/`dladdr`: `libshader_shading`'s `error()` is `libRIBCommon`'s and is this program's), case 3 (8 threads × 1000 reports while sinks swap) |
| same source, built with `CROSSLIB_STATIC` | `Common_DiagnosticsCrossLibraryStatic`: `ribRender_static` linked whole-archive, as in `openRenderConfig.cmake.in`; cases 0 and 2 |

`owned-symbols.txt` gains the four diagnostic functions, `installDiagnosticSink`,
`diagnosticsPrintDefault` and `current_log_level`.

**Red evidence**:
- `cmake --build build --target <t>` fails for all three targets with
  `fatal error: 'common/diagnostics.h' file not found`.
- `ctest -L common`: 1 of 9 tests passes (`Common_VariableDeclarations`).
  - The 5 `NoContext` tests fail.
  - The 2 `CrossLibrary` tests show "Not Run" (not built).
  - `Common_StateOwnership` fails: 7 of 11 symbols are not defined once, in `libRIBCommon`.
    - `error`/`warning`/`fatal`/`info` have 3 definers each: `libRIB{Render,Vector,Out}`.
    - `installDiagnosticSink` and `diagnosticsPrintDefault` are not defined anywhere yet.
    - `current_log_level` has **9** definers, one weak `inline` copy per image that includes
      `logging.hpp`. This confirms research R3's per-image copy concern.
- **Today's crash**: a throwaway program (`$S/023-throwaway/today_error.cpp`, not in the tree)
  calls today's `error()` with no RI context. It **dies with SIGSEGV (exit status 139)** before
  printing `CONTINUED`: the NULL `renderMan` dereference that US3 removes.
- **Deferred**: the three new test executables join `Common_StateOwnership`'s `IMAGES` at T021,
  once they build. The script rejects missing images, so they can't be listed earlier.
- **Observation, out of scope**: `includes/logging.hpp` also has `inline` instrumentation state
  (`_log_init_done`, `log_enabled`, `log_output`). Like `current_log_level`, each image gets its
  own copy, so `orender`'s `log_enabled`/`log_output` settings don't reach the libraries. Spec 023
  makes only `current_log_level` single-definition (research R3). The rest is a candidate
  follow-up for the maintainer to decide on.

### T019–T023: diagnostics in common (implementation)

**What changed**:

| Task | File(s) | Change |
|---|---|---|
| T019 | `src/common/diagnostics.h` | Created by `git mv` from `src/ri/parse/error.h`. `EErrorCode` kept verbatim, so mangled names are unchanged: `_Z5error10EErrorCodePKcz`, … Adds `EDiagnosticSeverity`, `CDiagnosticSink`, `installDiagnosticSink` and `diagnosticsPrintDefault` |
| T019 | `src/common/diagnostics.cpp` (new) | A `static std::atomic` sink slot; the four reporting functions, each formatting into a single `OS_MAX_PATH_LENGTH` buffer with `vsnprintf` (as before); `diagnosticsPrintDefault` holding `RiErrorPrint`'s stream rules |
| T020 | `src/common/logging.cpp` (new) | The only definition of `current_log_level`. `includes/logging.hpp` now declares it `extern` |
| T021 | `src/ri/parse/error.cpp` | Keeps `translate()`, unchanged. Adds `toRIE()`, the static `CRiDiagnosticSink` (forwards to `renderMan->RiError` while `renderMan != NULL`, otherwise uses the default rules) and `riDiagnosticsAttach`/`riDiagnosticsDetach` (declared in the new `src/ri/parse/riDiagnostics.h`) |
| T021 | `ri.cpp`, `riInterface.cpp`, `previewContext.cpp` | Attach/detach points (below). `RiErrorPrint` sets `RiLastError` as before, then calls `diagnosticsPrintDefault` |
| T022 | 66 files | `#include "error.h"` → `#include "common/diagnostics.h"`, including the `.y` grammar files. `parse/error.h` no longer exists, and no reference to it remains |

**Attach/detach design, refined while implementing**: the plan said "detach only in `RiEnd`", but
`renderMan` also goes NULL in `~CRiInterface`'s guard, in `dataLoad.cpp` and in
`previewContext.cpp`.
- **Attach**: the `CRiInterface` constructor, `RiContext(handle)` and `RiArchiveEnd` after it
  restores the outer context. Attach is idempotent.
- **Detach**:
  - in `~CRiInterface` when `renderMan == this`. This covers `RiEnd` and `dataLoad`, and the
    nested-archive delete, which `RiArchiveEnd` then re-attaches. It runs after the derived
    context's destructor, so end-of-render statistics still reach the handler;
  - after the preview's `renderMan = nullptr`.
- `contracts/common-api.md`, `data-model.md` and `research.md` (R3) were updated to match.

**Verification**:
- **Build**: clean under `-Werror` (no warnings).
  - One fix in my own test: `case1RiHandler`/`case3Threads` were unused in the
    `CROSSLIB_STATIC` build, so they are now guarded by it.
- **`ctest -L common`: 9/9 pass**:
  - the five `NoContext` tests (fatal ends with status 255 and its message on stderr);
  - `CrossLibrary` (cases 0–3) and `CrossLibraryStatic` (cases 0 and 2);
  - `StateOwnership`: all 11 symbols are defined once, in `libRIBCommon`, across 20 images,
    including the three new test executables. `current_log_level` went from 9 definers to 1;
  - `VariableDeclarations`.
- **A bug in my runner, found and fixed**: `diagnostics_runner.cmake`'s `expect` macro passed a
  quoted expression into `if()`, which CMake treats as a single string. Every check therefore
  reported failure, although the program's output was correct. It now uses plain `if()` checks.
  - **Negative check**: a wrapper that turns warning output into an `error` expectation is
    rejected ("marker unexpectedly on stdout / missing from stderr").
- **Architecture** (T023), before the prune: `violations=0 stale=94` — **84 DB-016** lines (45
  `error.h` includes + 39 diagnostic links) and **10 DB-001** lines (4 `error`, 5 `warning`,
  1 `info`), exactly as research R0 predicted.
  - Exactly the 94 reported lines were deleted (0 unmatched), and the section-header counts were
    updated:
    - DB-016: 215 → **131**;
    - DB-001: 30 → **20**;
    - allowlist: 686 → **592** lines.
  - archcheck: `RESULT PASS violations=0 stale=0`.
  - The metric is unchanged (TOTAL 813/118), as expected: no `CRenderer::` reference moved in
    this phase.
- **Output comparison** (T023), `$S/023-diagcmp/diag.rib`: a RIB that triggers an unknown option,
  an unknown attribute, a missing shader, an undeclared parameter, and a severe error from a
  malformed `Polygon`. Baseline install vs new build:
  - **stdout and stderr byte-identical**, at the default level and at `-x 3`;
  - the four messages keep their `./diag.rib (N):` RI-handler prefixes;
  - exit status 255 on both sides.
  - (A first comparison differed only because I pointed `DISPLAYS` at the wrong build directory.)
- **Full suite** (`nice -n 19 ctest --test-dir build -j4`): **567/567 pass** (109 s). That is Phase 2's 560 plus the 7 new diagnostics tests.
