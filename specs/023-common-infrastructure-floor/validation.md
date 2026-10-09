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

- **Linux**: for the maintainer to run on the Linux host, from master's `build-022`. Paste the
  results into `tools/libshader-undef-before-linux.txt` and
  `tools/libshader-bindings-before-linux.txt`:

  ```bash
  cd /home/juvenal/Projects/Development/CLI/openrender
  nm -D --undefined-only build-022/src/libshader/shading/libshader_shading.so.1 | c++filt \
    > /tmp/023-libshader-undef-before-linux.txt
  SHADERS="$PWD/build-022/shaders" ORENDERHOME="$PWD/openrender" \
  LD_DEBUG=bindings build-022/src/orender/orender examples/rib/camera-dof.rib 2>&1 \
    | grep 'file=.*libshader_shading' \
    | grep -E 'error|warning|info|stats|memory|globalMemory|parseVariable|sample.*Hemisphere|CRefCounter|CFileResource' \
    > /tmp/023-libshader-bindings-before-linux.txt
  wc -l /tmp/023-libshader-*-before-linux.txt
  ```

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
