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
