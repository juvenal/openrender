---

description: "Task list for spec 023: common infrastructure floor"
---

# Tasks: Common Infrastructure Floor

**Input**: Design documents from `specs/023-common-infrastructure-floor/`

**Prerequisites**:
- plan.md
- spec.md
- research.md (R0–R14)
- data-model.md
- contracts/common-api.md
- contracts/variable-lookup.md
- contracts/library-abi.md
- quickstart.md

**Tests**: REQUIRED. Constitution III (TDD) is non-negotiable. In every phase the tests are written
first and fail before the implementation tasks that make them pass. The exceptions are the golden
and consistency tables, which must pass on the old code and keep passing.

Phases 7 and 8 add only verification-only tooling: T052's scratch `dlopen` program and T055's
`perf_ab.sh`. They measure what earlier phases built and drive no implementation, so they have no
red-then-green cycle and no TEST-APPROVAL gate. They are reviewed at that phase's PAUSE (7 or 8)
instead.

**Organization**: phases follow plan.md's implementation sequence. Diagnostics (US3) come before
the generic-core moves (US2) because `libRIBCommon` may not reference symbols upward (research R1).
Every phase ends in two pauses:
- a **TEST-APPROVAL** pause after its tests-first block;
- a **review/commit PAUSE** at its end, where ctest results are reported, changes are staged, and
  the maintainer reviews and commits.

The agent never commits.

**Working directory**: `/Volumes/Projects/Development/CLI/openrender-worktrees/023-common-infrastructure-floor`,
with build tree `build/`. Paths below are repo-relative. `$S` is the session scratchpad.

**Hard rule**: every build is `nice -n 19 cmake --build build --config Release -j4`, and every
ctest run is `nice -n 19 ctest --test-dir build … -j4`. Never use a higher `-j`, or `-j` with no
number. Performance runs are serial.

**Red tests that don't compile yet**: a new test that needs an API that doesn't exist yet is added
as `EXCLUDE_FROM_ALL`, so the main build stays green.
- At its TEST-APPROVAL pause, the red state is shown two ways: `cmake --build build --target <t>`
  fails to compile, and the ctest entry is "Not Run" or failed.
- The implementation task that provides the API removes `EXCLUDE_FROM_ALL`.

**Allowlist hygiene in every phase**: after a phase's moves, run `ctest -L architecture`, then:
- delete exactly the allowlist lines it reports **stale**; each must be a `DB-016` or `DB-001`
  line;
- lower `tests/architecture/metrics-baseline.txt` to the reported values. A value may only go down;
  if one rises, stop and investigate;
- re-run until green.

**Adding a line is never allowed** (FR-010). If the check reports a **new** violation, the design
is wrong: stop and report it; don't allowlist it.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency on unfinished tasks)
- **[Story]**: US1–US4 from spec.md

---

## Phase 1: Setup — baselines (before ANY source edit)

**Purpose**: capture everything that can only be captured before the feature exists (research R11).

- [X] T001 Confirm the baseline is pristine.
  - `git diff --stat 010ce04 -- src tests CMake CMakeLists.txt` must print nothing; only `specs/`
    may differ.
  - `nice -n 19 cmake --build build --config Release -j4` must be a no-op.
  - Record `git rev-parse HEAD` and the build type in a new
    `specs/023-common-infrastructure-floor/validation.md`.
- [X] T002 Create the frozen baseline install.
  - Snapshot: `ls -laR /usr/local > $S/023-usrlocal-before.txt`.
  - Install: `DESTDIR=$S/023-base nice -n 19 cmake --install build`.
  - Snapshot again to `$S/023-usrlocal-after.txt` and `diff` the two. Any difference means stop
    and report (memory: install(CODE) once leaked outside DESTDIR).
  - Record the install's lib, bin, shaders and displays directories in `validation.md`.
- [X] T003 [P] Write the SC-006 compatibility consumer under
  `specs/023-common-infrastructure-floor/tools/compat-consumer/`.
  - `consumer.cpp` uses only the installed `ri.h`: `RiErrorHandler(RiErrorPrint)`, `RI_*` tokens
    (`RI_P`, `RI_CS`, `RI_RGBA`, `RI_FILE`), `RiDeclare("foo", "uniform float")`, and a
    `RiSphere` in a world block.
  - The library under test is chosen at link time:
    - `libRIBRender`: `RiBegin(RI_NULL)`, then before `RiWorldBegin`:
      `RiOption("limits", "numthreads", &one)` (one = 1), `RiPixelSamples(1, 1)`,
      `RiFormat(64, 64, 1)`, then `RiDisplay("out-RIBRender-<tag>.tif", "file", "rgba")`.
      A single thread and one sample per pixel remove the run-to-run variation from per-thread
      random numbers and dynamic bucket assignment. This also exercises the installed file display
      driver.
    - `libRIBVector` and `libRIBOut`: `RiBegin("out-<lib>-<tag>.rib")`. Memory warns that
      `RiBegin(RI_NULL)` crashes on those two.
  - `build.sh <install-root> <out-dir>` compiles three binaries, `consumer-RIBRender`,
    `consumer-RIBVector` and `consumer-RIBOut`, each linked against exactly one of the baseline RI
    libraries in `<install-root>`, with `-Wl,-rpath,<install-root>/<prefix>/lib`.
  - `run.sh <install-root> <tag> [--pinned]` runs all three.
    - It sets `DISPLAYS`, `SHADERS` and `ORENDERHOME` to the install's displays, shaders and home
      directories.
    - By default it sets **no** `DYLD_*`/`LD_*` library-path variables, so the binaries resolve
      their libraries only through their own rpath.
    - It prints the resolved libraries once (`DYLD_PRINT_LIBRARIES=1` or `ldd`), and fails if any
      `libRIB*` resolves outside `<install-root>` or any run exits non-zero.
    - `--pinned` additionally exports `DYLD_LIBRARY_PATH`/`LD_LIBRARY_PATH` to the install's lib
      dir. It is a diagnostic aid only, never the proof.
  - `render-installed.sh <install-root>` runs the install's own `orender` on
    `examples/rib/camera-dof.rib`, with the same pinning plus `SHADERS`/`ORENDERHOME` set to the
    install. It expects exit 0 and a TIFF.
  - `build-package.sh <install-root> <out-dir>` builds a two-target out-of-tree project with
    `find_package(openRender COMPONENTS Shared Static)`: one target linking `openRender::RIBRender`
    and one linking `openRender::RIBRender_static`. Both must render a sphere.
- [X] T004 Build and run the baseline consumers, keeping the binaries **unchanged** for Phase 7.
  - Create the in-place root, a copy of the frozen install that T051 later installs the new
    libraries over: `cp -a $S/023-base $S/023-inplace`. `$S/023-base` itself stays untouched as
    the performance A side.
  - `tools/compat-consumer/build.sh $S/023-inplace $S/023-compat`: the binaries are built **now,
    before any source change**, with an rpath into `$S/023-inplace`.
  - `$S/023-compat/run.sh $S/023-inplace base`
  - `render-installed.sh $S/023-base`
  - `build-package.sh $S/023-base $S/023-pkg-base`

  Record the outputs and exit codes in `validation.md`.
- [X] T004a Determinism probe: run `$S/023-compat/run.sh $S/023-inplace base2` and compare every
  `out-*-base` file with `out-*-base2` using `cmp`.
  - If all files match, T051 compares bytes.
  - If any TIFF differs, record it in `validation.md`. T051 then compares that TIFF with the
    visual-test 8×8 block-average metric (the `tests/visual` comparator, threshold 0), and exact
    `cmp` applies only to the RIB outputs.
  - **Result (2026-10-09)**: the TIFF is byte-identical. The RIB outputs differ only in the
    `##CreationDate` header line, which `libRIBOut`/`libRIBVector` stamp with the wall-clock time.
    The rule is encoded in `tools/compat-consumer/compare.sh <dir> <tag-a> <tag-b>`: `cmp` for
    `.tif`, `diff` with `##CreationDate` excluded for `.rib`.
- [X] T005 [P] Copy the macOS "before" list `$S/023-libshader-undef-before.txt` to
  `specs/023-common-infrastructure-floor/tools/libshader-undef-before-macos.txt`, so it survives
  the session.
  - Add quickstart §0's Linux "before" commands to `validation.md` as a block for the maintainer
    to run on `build-022`.
  - The maintainer pastes the results into `tools/023-libshader-undef-before-linux.txt` and
    `tools/023-libshader-bindings-before-linux.txt`.
  - **2026-10-09**: the undefined-symbol list is in (463 lines, all 17 symbols). The bindings file
    came back empty because the first command was wrong; `validation.md` has the corrected
    `LD_BIND_NOW=1` command, to be re-run before Phase 7 needs it.
- [X] T006 Write the golden variable-declaration test, `Common_VariableDeclarations` (label
  `common`, FR-005), against the **current** `src/ri/parse/variable.h`.
  - Create `tests/unit/common/CMakeLists.txt` and add `add_subdirectory(unit/common)` to
    `tests/CMakeLists.txt` after `unit/rib_parse`.
  - `tests/unit/common/test_common_variables.cpp` has a `--dump` mode that runs `parseVariable` over
    a declaration matrix and prints every `CVariable` field plus the return value. The matrix covers:
    - storage classes: none, `uniform`, `varying`, `vertex`, `facevarying`, `constant`;
    - types: `float`, `color`, `point`, `vector`, `normal`, `matrix`, `string`, `integer`, `int`,
      `hpoint`, `double`;
    - array forms: none, `[1]`, `[4]`;
    - invalid forms: unknown type, unknown class, bad array syntax, empty string, a name longer than
      `VARIABLE_NAME_LENGTH - 1`. If the long name would trip the lexer `assert`, test a name
      exactly at the limit instead.

    The test links `ribRender openrendercommon`.
  - Run `--dump` once on the baseline and commit its output verbatim as
    `tests/unit/common/variable_golden.txt`, a plain-text table the test reads at runtime, so it
    diffs cleanly in review. The test compares the live result to it.
  - It must **pass now**; that is the baseline for the Phase 5 move.

**Checkpoint (TEST-APPROVAL 1a + PAUSE 1)**: the compatibility test programs (T003) and the golden
variable test (T006) are this feature's first tests.
- Baselines are captured and recorded in `validation.md`, including T004a's determinism result.
- `Common_VariableDeclarations` passes on the old code.
- The full suite is unchanged.

Show T006 passing and T004/T004a's baseline outputs. Stage, and wait for the maintainer to approve
these tests, review and commit. Hand the Linux "before" block to the maintainer.

---

## Phase 2: Foundational — `libRIBCommon` becomes the shared owner (packaging only, no symbol moves)

**Purpose**: research R1/R2 and contracts/library-abi.md §1, §2, §5. It blocks every story.

**⚠️ CRITICAL**: no story work begins until this phase is complete.

### Tests first

- [X] T007 Write the state-ownership check, `Common_StateOwnership` (research R2, contracts/library-abi.md §2).
  - Files: `tests/unit/common/state_ownership.cmake` (a `cmake -P` script) and
    `tests/unit/common/owned-symbols.txt`, one mangled symbol per line with a `#` comment.
  - The script gets `-DIMAGES=<;-list>` and `-DCOMMON=<path to libRIBCommon>` from
    `tests/unit/common/CMakeLists.txt`. Build `IMAGES` from `$<TARGET_FILE:…>` of every shared
    library and executable target in the tree: the `libRIB*` shared libraries, `libshader_*`
    shared libraries, the display modules `file`, `framebuffer`, `rgbe` and (when OpenEXR is
    found) `openexr`, `orender`, `oshader`, `otexmake`, `precomp`, `sloinfo`, and the test
    executables of `tests/unit/common`.
  - Each listed symbol must have **exactly one** defining image, and it must be `COMMON`.
    - macOS: `nm -gU`.
    - Linux shared objects: `nm -D --defined-only`.
    - Linux executables: a definition counts as a reference if `readelf -rW <exe>` shows an
      `R_*_COPY` relocation for that symbol.
  - Seed `owned-symbols.txt` with symbols that are already common today but get embedded into
    several images: `RiBezierBasis`, `RiGaussianFilter`, `RiCatmullRomFilter` (from
    `src/common/rslConstants.cpp`), plus one `os*` function, e.g. `osCPUTime`.
  - Later phases append their moved symbols (T014, T026, T037).
  - **Expected red now**: `libRIBCommon` doesn't exist as a shared library, and the seed symbols
    are defined in several images.

**Checkpoint (TEST-APPROVAL 2a)**: `Common_StateOwnership` is written and failing. Show its output.
Stage, and wait for approval of the test before T008.

### Implementation

- [X] T008 Restructure `src/common/CMakeLists.txt`.
  - Build an OBJECT library `openrendercommon_obj` (PIC) with today's sources, include directories
    and dependencies.
  - Target **`openrendercommon`** becomes `SHARED`:
    - `OUTPUT_NAME RIBCommon`, `EXPORT_NAME RIBCommon`;
    - `VERSION`/`SOVERSION ${OPENRENDER_COMPAT_SOVERSION}`;
    - install name `@rpath` under `INSTALL_SELFCONTAINED` and `${CMAKE_INSTALL_PREFIX}/lib`
      otherwise, mirroring `ribRender` at `src/ri/CMakeLists.txt:237-246`;
    - link options `LINKER:-undefined,error` (APPLE) or `LINKER:--no-undefined` (Linux) (research
      R1).
  - New target **`openrendercommon_static`**: `STATIC`, `OUTPUT_NAME RIBCommon`,
    `EXPORT_NAME RIBCommon_static`.
  - Keep any other target that links the OBJECT library's dependencies (e.g. pthread, dl) working.
- [X] T009 Re-export spike on `ribRender` in `src/ri/CMakeLists.txt` (research R1).
  - Add `target_link_options(ribRender PRIVATE "LINKER:-reexport_library,$<TARGET_FILE:openrendercommon>")`
    under `if(APPLE)`, and keep `openrendercommon` in `target_link_libraries`.
  - Rebuild, then check `otool -l build/src/ri/libRIBRender.1.dylib | grep -A2 LC_REEXPORT_DYLIB`.
  - If ld64 warns or fails on the duplicate, drop `openrendercommon` from `target_link_libraries`
    for APPLE and add `add_dependencies(ribRender openrendercommon)`.
  - Record the outcome in `research.md` R1 ("spike result").
- [X] T010 Apply T009's working form to `ribVector` and `ribOut` in `src/ri/CMakeLists.txt`.
  - `ribRender_static` (`:252`) keeps `openrendercommon`, the shared library (research R1).
  - `ribVector_static` (`:434`) and `ribOut_static` (`:554`) switch to `openrendercommon_static`.
- [X] T011 [P] Make the libshader libraries link the shared library instead of embedding the archive.
  - `src/libshader/compiler/CMakeLists.txt:133` and `src/libshader/runtime/CMakeLists.txt:37`:
    replace `PUBLIC $<BUILD_INTERFACE:openrendercommon>` with `PUBLIC openrendercommon`, and
    update the comments at `:131`/`:35`.
  - The `*_static` variants (`compiler:158`, `runtime:52`) switch to `openrendercommon_static`.
  - `src/libshader/shading/CMakeLists.txt:119` keeps `PRIVATE openrendercommon`, which is now
    shared. Its `-undefined dynamic_lookup` / `--allow-shlib-undefined` stay; spec 029 removes
    them.
- [X] T012 [P] Install and package.
  - In `src/common/CMakeLists.txt`, install `openrendercommon` with `EXPORT openRenderTargets`
    (LIBRARY/RUNTIME/ARCHIVE to the standard dirs) and `openrendercommon_static` with
    `EXPORT openRenderStaticTargets`.
  - Remove the old static-only install rule wherever it lives. Search for `openrendercommon` in
    the root `CMakeLists.txt:637-677` and `src/common/CMakeLists.txt`.
  - In `CMake/openRenderConfig.cmake.in`:
    - the `Static` component must include `openRenderTargets.cmake` first, because
      `RIBRender_static` needs the shared `openRender::RIBCommon`;
    - keep the `-force_load`/`--whole-archive` block (`:62-78`) unchanged;
    - update its comment to say the infrastructure now comes from `libRIBCommon`, and only DB-001's
      renderer symbols need the whole archive.
- [X] T013 [P] Bundling and runtime search paths.
  - `src/preview/orender-wire-macos/CMakeLists.txt:51-90`: copy
    `libRIBCommon.${OPENRENDER_COMPAT_SOVERSION}.dylib` and its unversioned symlink into
    `Contents/Frameworks`, exactly like `libRIBVector`, and add `openrendercommon` to the bundle
    target's dependencies.
  - Check that the Linux preview target finds the library through the build RPATH.
  - Check that the display modules (`src/display/{file,framebuffer,openexr,rgbe}/`) installed into
    the displays directory resolve `@loader_path/../lib` / `$ORIGIN/../lib` to the lib dir
    holding `libRIBCommon`. Set `INSTALL_RPATH` on those targets if the displays dir isn't a
    sibling of `lib/`.
  - Check that the `INSTALL_SELFCONTAINED` dependency-copy block (root `CMakeLists.txt:682-…`)
    doesn't need changes for an in-tree library.
- [X] T014 Build, then verify:
  - `otool -L` shows `libRIBCommon` on `orender`, `libRIBRender`, `libshader_shading`,
    `libshader_compiler` and a display driver;
  - `LC_REEXPORT_DYLIB` is present on all three RI libraries;
  - `Common_StateOwnership` is green for the seed list;
  - `ctest -L architecture` is green; nothing should change, since no symbol moved;
  - the full suite is unchanged;
  - `otool -L build/orender-wire.app/Contents/MacOS/orender-wire` lists `libRIBCommon` through
    `@rpath`, and the bundle's `Contents/Frameworks` contains
    `libRIBCommon.${OPENRENDER_COMPAT_SOVERSION}.dylib`.

**Checkpoint (PAUSE 2)**:
- `libRIBCommon` is shared, the single definer of the seed symbols, and re-exported;
- the full suite is unchanged.

Stage, summarize (including T009's spike result), and wait for review and commit.

---

## Phase 3: User Story 3 — Diagnostics work without an RI context (Priority: P2; sequenced first, see header)

**Goal**: the diagnostics service lives in common with an installable sink. The default sink
follows `RiErrorPrint`'s rules, and Scene Interface attaches the RI sink while a context exists
(FR-001–FR-003, FR-015).

This phase also removes the 45 `error.h` include edges and the `error`/`warning`/`info` link edges
(US1).

**Independent test**: `Common_DiagnosticsNoContext_*` and `Common_DiagnosticsCrossLibrary{,Static}`
pass; the full suite is unchanged (SC-004).

### Tests first

- [X] T015 [P] [US3] Write the no-context diagnostics test (`EXCLUDE_FROM_ALL` until T021).
  - `tests/unit/common/test_common_diagnostics.cpp` takes a mode argument: `error`, `warning`,
    `info`, `info_quiet` (sets `current_log_level` to WARNING first), `fatal`. Each reports one
    diagnostic carrying a unique marker string, and the program then prints `CONTINUED` to stdout
    and returns 0.
  - `tests/unit/common/diagnostics_runner.cmake` uses `execute_process` with
    `OUTPUT_VARIABLE`/`ERROR_VARIABLE`/`RESULT_VARIABLE` and asserts per mode (SC-003):

    | Mode | Marker appears on | `CONTINUED` | Exit status |
    |---|---|---|---|
    | `error` | stderr only | yes | 0 |
    | `warning` | stdout only | yes | 0 |
    | `info` | stdout | yes | 0 |
    | `info_quiet` | nowhere | yes | 0 |
    | `fatal` | stderr | no | 255 (`exit(-1)`) |

  - Register `Common_DiagnosticsNoContext_{error,warning,info,info_quiet,fatal}`.
  - Today an `error()` call with no context dereferences a NULL `renderMan`, so the tests are red.
- [X] T016 [P] [US3] Write the cross-library test, `Common_DiagnosticsCrossLibrary`
  (`tests/unit/common/test_common_crosslib.cpp`, `EXCLUDE_FROM_ALL` until T021). It links
  `ribRender libshader_shading openrendercommon`.
  - **Case 0**: install a capture sink, then run two RI sessions in a row
    (`RiBegin(RI_NULL)`/`RiEnd()` twice).
    - Before the first session, between the sessions and after the last one,
      `error(CODE_BUG, "between\n")` must reach the capture sink.
    - After the last `RiEnd`, `installDiagnosticSink(x)` returns the capture sink.
    - This covers the idempotent attach/detach in data-model.md and the spec's
      "several RI sessions" edge case.
  - **Case 1**: `RiBegin(RI_NULL)` → `RiErrorHandler(captureHandler)` →
    `error(CODE_BADTOKEN, "m1\n")` → the handler receives `RIE_BADTOKEN`, `RIE_ERROR` and
    `"m1\n"` (`error.cpp:101-102` maps `CODE_BADTOKEN` to `RIE_BADTOKEN`).
    - Repeat with `warning(CODE_BADTOKEN, "m2\n")` → `RIE_BADTOKEN`/`RIE_WARNING`.
    - Repeat with `info(CODE_BADTOKEN, "m3\n")` → `RIE_BADTOKEN`/`RIE_INFO`.
    - The constants come from the public `ri.h`. Then `RiEnd()`.
  - **Case 2**: obtain the `error` that `libshader_shading` binds to:
    `dlopen` libshader_shading, then `dlsym(handle, "_Z5error10EErrorCodePKcz")`. On macOS, dlsym
    with a handle searches that image and its dependents.
    - `dladdr(ptr).dli_fname` must end in `libRIBCommon…`.
    - `ptr` must equal `&error` as seen by the test.
    - Calling it with a capture sink installed must reach the capture sink.
  - **Case 3 (threads)**: 8 `std::thread`s each report 1000 `info()` diagnostics into a
    thread-safe capture sink, while the main thread swaps between two capture sinks 1000 times
    with `installDiagnosticSink`.
    - Assert the two capture sinks' totals sum to 8000, with no crash.
    - On Linux, the maintainer may run this case once under `-fsanitize=thread`; this is optional
      and reported in `validation.md`.
- [X] T017 [P] [US3] Write `Common_DiagnosticsCrossLibraryStatic`
  (`tests/unit/common/test_common_crosslib_static.cpp`, `EXCLUDE_FROM_ALL` until T021).
  - Same cases 0 and 2, linked the way the package `Static` component links: `ribRender_static`
    with `-force_load`/`--whole-archive` (copy the logic of `openRenderConfig.cmake.in:62-78`),
    plus `openrendercommon` (shared) and `libshader_shading`.
  - Add the executable to `IMAGES` in T007's registration. It must **not** define any owned
    symbol.
- [X] T018 [P] [US3] Append the diagnostics symbols to `tests/unit/common/owned-symbols.txt`:
  - `_Z5error10EErrorCodePKcz`, `_Z7warning10EErrorCodePKcz`, `_Z5fatal10EErrorCodePKcz`,
    `_Z4info10EErrorCodePKcz`;
  - the mangled `installDiagnosticSink` and `diagnosticsPrintDefault`;
  - `current_log_level`.

  The sink slot itself is `static` inside `diagnostics.cpp`; its single definer is proven by
  `installDiagnosticSink`'s.

**Checkpoint (TEST-APPROVAL 3a)**:
- T015–T017 are written and red: show the compile failures from `--target` builds, and the no-context
  crash from today's `error()` (run a throwaway build of T015 against today's API if it compiles).
- `Common_StateOwnership` is red for the new symbols.

Stage, and wait for approval before T019.

### Implementation

- [X] T019 [US3] Create the common diagnostics files (contracts/common-api.md §1).
  - `git mv src/ri/parse/error.h src/common/diagnostics.h` (keeps history), then rewrite it to the
    contract:
    - `EErrorCode` verbatim (`error.h:33-67`);
    - `EDiagnosticSeverity`;
    - `CDiagnosticSink`;
    - `installDiagnosticSink`, `diagnosticsPrintDefault`;
    - `error`/`warning`/`fatal`/`info` with unchanged signatures.
  - New `src/common/diagnostics.cpp`:
    - a `static std::atomic<CDiagnosticSink *>` slot, constant-initialized to `nullptr`;
    - the four functions format exactly like `src/ri/parse/error.cpp:157-213`: same
      `OS_MAX_PATH_LENGTH` buffer and `vsnprintf`;
    - `diagnosticsPrintDefault` holds the stream rules copied from `ri.cpp:2350-2380`, without
      `RiLastError`.
- [X] T020 [US3] Make `current_log_level` a single definition (research R3).
  - In `src/includes/logging.hpp:28`, change `inline LogLevel current_log_level = LogLevel::INFO;`
    to `extern LogLevel current_log_level;`.
  - New `src/common/logging.cpp` defines it with the same initial value.
  - Add `diagnostics.cpp` and `logging.cpp` to `src/common/CMakeLists.txt`.
- [X] T021 [US3] Scene Interface side (contracts/common-api.md §1 "Scene Interface implementation").
  - `src/ri/parse/error.cpp`:
    - keep `translate()`;
    - delete the four functions;
    - add the static `CRiDiagnosticSink`, `toRIE()`, and `riDiagnosticsAttach()` /
      `riDiagnosticsDetach()`, declared in new `src/ri/parse/riDiagnostics.h`. Attach saves the
      previous pointer once; detach restores it.
  - `src/ri/parse/riInterface.cpp:36`: call `riDiagnosticsAttach()` after `renderMan = this;`.
  - `src/ri/parse/ri.cpp`:
    - call `riDiagnosticsDetach()` right after `renderMan = NULL;` (`:778`), and nowhere else;
    - in `RiErrorPrint` (`:2350`), set `RiLastError = code` for ERROR/SEVERE, then call
      `diagnosticsPrintDefault(severity mapped from RIE_*, message)`. Output bytes are unchanged.
  - Remove `EXCLUDE_FROM_ALL` from T015–T017, and add `test_common_diagnostics`,
    `test_common_crosslib` and `test_common_crosslib_static` to `Common_StateOwnership`'s `IMAGES`
    (`tests/unit/common/CMakeLists.txt`). They are left out until they build, because the script
    rejects an image that does not exist.
- [X] T022 [US3] Switch every includer of the old header (67 files: `grep -rln '"error.h"\|parse/error.h' src tests`)
  to `#include "common/diagnostics.h"`.
  - This includes the grammar sources `rib.y`, `dso.y`, `ifexpr.y`, `variable.y`, `rslo.y` and the
    `.l` files.
  - Grouped by directory, these can be done in parallel by hand; it's one sed pass plus a review.
  - Confirm `src/ri/parse/error.h` no longer exists and nothing references it.
- [X] T023 [US3] Build, then:
  - run `ctest -L common`, which must be green, including the ownership check for the new symbols;
  - run `ctest -L architecture`, prune the stale lines (the `error.h` includes and the
    `error`/`warning`/`info` link lines under DB-016 and DB-001), and re-run until green;
  - run the full suite, which must be unchanged;
  - spot-check that the text output of `orender` on `examples/rib/camera-dof.rib` (stdout and
    stderr, with timing lines filtered out) is identical to T002's install. Images are covered by
    the visual suite's thresholds, not compared byte for byte;
  - record the line counts removed in `validation.md`.

**Checkpoint (PAUSE 3)**:
- diagnostics come from common;
- no-context and cross-library tests are green;
- allowlist and metric are pruned;
- the suite is unchanged.

Stage, summarize, and wait for review and commit.

---

## Phase 4: User Story 2 — The shading library gets its infrastructure from common (Priority: P1)

**Goal**: move the generic `src/ri/core` code into common and replace `CRenderer::globalMemory`
with `globalArena` (FR-006, FR-007; research R6, R7).

This also delivers US1's arena and `globalMemory` edges.

**Independent test**: `Common_ShadingInfraBinding` and `Common_ArenaSemantics` pass, and the
ownership check is green for the 17 symbols.

### Tests first

- [ ] T024 [P] [US2] Write `Common_ArenaSemantics` (`tests/unit/common/test_common_arena.cpp`,
  `EXCLUDE_FROM_ALL` until T030), testing a local `CArena`:
  - `init()` makes `top` non-null;
  - nested `arenaBegin`/`arenaEnd` restore `top`, `memory` and `availableSize`;
  - `arenaSave`/`arenaRestore` round-trip;
  - an allocation larger than the page grows a new page, and `arenaEnd` unwinds it;
  - `ralloc` is 8-byte aligned;
  - `rstrdup` copies;
  - `tini()` leaves `top == nullptr`.
- [ ] T025 [P] [US2] Write `Common_ShadingInfraBinding` (`tests/unit/common/shading_infra_binding.cmake`,
  `cmake -P`), following contracts/library-abi.md §3.
  - **macOS**: `nm -m` on `$<TARGET_FILE:libshader_shading>`. Each of the 17 symbols (the list
    from spec US2, mangled, kept in `tests/unit/common/shading-infra-symbols.txt`) must be defined
    locally or `(from libRIBCommon)`, never `(dynamically looked up)`.
  - **Linux**: `readelf -d` lists `libRIBCommon.so.1`, and `nm -D --defined-only` of
    `libRIBRender.so` defines none of the 17.
  - **Expected red** now for the 14 symbols not yet moved; `error`, `warning` and `info` already
    pass after Phase 3.
- [ ] T026 [P] [US2] Append to `owned-symbols.txt`:
  - `globalArena`, `stats`;
  - `memoryInit`, `memoryTini`, `memoryNewPage`, `memoryDeletePage`;
  - `sampleHemisphere`, `sampleCosineHemisphere`;
  - the `CRefCounter`/`CFileResource` constructors, destructors, vtables and typeinfo (mangled).

**Checkpoint (TEST-APPROVAL 4a)**: T024–T026 are red. Show the output, stage, and wait for approval
before T027.

### Implementation

- [ ] T027 [US2] Move the generic core files with `git mv src/ri/core/{refCounter.h,refCounter.cpp,fileResource.h,fileResource.cpp,memory.h,memory.cpp,stats.h,stats.cpp,random.h,random.cpp,atomic.h} src/common/`.
  - Add the `.cpp` files to `src/common/CMakeLists.txt`.
  - Remove them from `ri_sources` in `src/ri/CMakeLists.txt` (`:76,94,106,107,129`) and from the
    ribOut whitelist (`:494,501`). Update the `core/` comment at `:30`.
  - Fix intra-common includes, e.g. `stats.cpp` → `"common/diagnostics.h"` and `refCounter.h` →
    `"common/atomic.h"`.
- [ ] T028 [US2] Split `ri_config.h`: move `ZONE_INIT_SIZE`/`ZONE_BASE_SIZE`/`ZONE_INCREMENT_SIZE`
  from `src/ri/core/ri_config.h` into `src/common/memory.cpp`, and drop `memory.cpp`'s
  `#include "ri_config.h"`. `VARIABLE_NAME_LENGTH` moves in Phase 5.
- [ ] T029 [US2] Switch every includer of the moved headers to `"common/<name>.h"`:
  - `memory.h` (≈49), `stats.h` (≈47), `random.h` (≈22), `atomic.h` (≈10), `refCounter.h` (≈9),
    `fileResource.h` (≈8);
  - use `grep -rln '"<name>.h"\|core/<name>.h'` across `src/` and `tests/`.

  Watch for the system `<memory>`, which must be left alone.
- [ ] T030 [US2] Add `CArena`, the `ralloc`/`rstrdup` `CArena&` overloads, the
  `arenaBegin`/`arenaEnd`/`arenaSave`/`arenaRestore` macros and `extern CArena globalArena;` to
  `src/common/memory.h`, exactly as contracts/common-api.md §2 specifies.
  - Define `CArena globalArena;` plus `CArena::init()`/`tini()` in `src/common/memory.cpp`.
  - Remove `EXCLUDE_FROM_ALL` from T024.
- [ ] T031 [P] [US2] Replace `CRenderer::globalMemory` in Scene Interface:
  - `src/ri/parse/{rib.y,rib.l,ifexpr.y,ifexpr.l,ri.cpp}`;
  - `ri.cpp:727/733` become `globalArena.init()`/`tini()`;
  - `memBegin(X)`→`arenaBegin(globalArena)`, `memEnd`→`arenaEnd`, `ralloc(n, X)`→`ralloc(n, globalArena)`,
    `rstrdup`, `memSave`/`memRestore`→`arenaSave`/`arenaRestore`.
- [ ] T032 [P] [US2] Apply the same replacement to Texturing: `src/ri/texture/{texmake.h,texmake.cpp,texture.cpp}`.
- [ ] T033 [P] [US2] Apply the same replacement to Hiders: `src/ri/hiders/{occlusion.cpp,zbuffer.cpp,stochastic.cpp}`.
- [ ] T034 [P] [US2] Apply the same replacement to Shading.
  - `src/libshader/shading/{rslo.y,rslo.l,shading.cpp}`: `shading.cpp:435` uses `globalArena.top`
    instead of `s_defaultServices->globalMemory()`.
  - Delete `virtual CMemPage *globalMemory() const = 0;` from
    `src/libshader/shading/rendererServices.h:58` and its override from
    `src/ri/render/rendererServicesImpl.h:38` (contracts/variable-lookup.md §2).
- [ ] T035 [P] [US2] Apply the same replacement to the renderer core and consumers.
  - `src/ri/render/{rendererContext.cpp,renderer.cpp,rendererMutexes.cpp(comment),init.cpp}`:
    `renderer.cpp:93/152` become `init()`/`tini()`.
  - `src/ri/core/pl.cpp`, which is still in core in this phase.
  - `src/ri/dataviewer/dataLoad.cpp:143/166` and `src/preview/libribpreview/previewContext.cpp:191/205`
    become `init()`/`tini()`.
  - Delete `init.cpp:59`'s `#define threadMemory CRenderer::globalMemory` and write its uses out
    against `globalArena` (FR-009).
  - Delete `static CMemPage *globalMemory` from `src/ri/render/renderer.h:140` and its definition
    from `rendererStatics.cpp:106`.
  - Confirm `grep -rn "globalMemory" src` returns only comments, then reword those comments.
- [ ] T036 [US2] Build, then:
  - `ctest -L common`, all green, including `ShadingInfraBinding` and `StateOwnership`;
  - `ctest -L architecture`: prune the stale `globalMemory`/`renderer.h` lines, lower
    `metrics-baseline.txt` (every domain drops by its `globalMemory` references; none rises), and
    re-run until green;
  - the full suite, unchanged.

  Record the counts in `validation.md`.

**Checkpoint (PAUSE 4)**:
- the generic core and the arena are in common;
- the shading library binds the 17 infrastructure symbols to `libRIBCommon`;
- the suite is unchanged.

Stage, summarize, and wait for review and commit.

---

## Phase 5: User Story 1 — Vocabulary and variable declarations come from common (Priority: P1)

**Goal**: move `rendererc.h` to common, add `riVocabulary.h`/`riTokens.cpp`, and move the variable
grammar to common (FR-004, FR-005; research R4, R5).

**Independent test**: `Common_VocabularyConsistency` and `Common_VariableDeclarations` (unchanged
golden) pass, and the DB-016 include and token lines become stale.

### Tests first

- [ ] T037 [P] [US1] Write `Common_VocabularyConsistency`.
  - `tests/unit/common/vocabulary_consistency.cmake` (`cmake -P`) extracts three token-name sets:
    - from `src/ri/parse/ri.h`'s `EXTERN(RtToken)` declarations;
    - from `src/common/riVocabulary.h`;
    - from `src/common/riTokens.cpp`'s definitions.

    The sets must be equal. Report the symmetric difference on failure.
  - `tests/unit/common/test_vocabulary_both_headers.cpp` includes both `ri/parse/ri.h` and
    `common/riVocabulary.h` and checks the address of one token from each. It is
    `EXCLUDE_FROM_ALL` until T039.
  - Make the ownership script also read every `RI_*` name from `ri.h`, so all tokens become owned
    symbols; add `_Z13parseVariableP9CVariablePKcS2_` to `owned-symbols.txt`.
  - Red now.

**Checkpoint (TEST-APPROVAL 5a)**: T037 is red. The golden test (T006) is still green. Stage, and
wait for approval before T038.

### Implementation

- [ ] T038 [US1] `git mv src/ri/render/rendererc.h src/common/rendererc.h`. Switch every includer
  (≈12, `grep -rln '"rendererc.h"'`) to `"common/rendererc.h"`. That covers `common/colorSpace.h`,
  `variable.h`, `pl.h`, the libshader headers and `ifexpr.y`.
- [ ] T039 [US1] Create `src/common/riVocabulary.h` (contracts/common-api.md §4).
  - `extern "C"` declarations of the `Rt*` typedefs and RI macros, character-identical to
    `ri.h`'s type section.
  - Every `RI_*` token, as `extern RtToken RI_X;`, in `ri.h`'s order.
  - The basis matrices and filter functions defined in `src/common/rslConstants.cpp`.
  - No `EXTERN` or `RENDERER` dependency.
- [ ] T040 [US1] Create `src/common/riTokens.cpp` by moving the token definitions verbatim from
  `src/ri/parse/ri.cpp:51-372`.
  - Delete them from `ri.cpp`.
  - Add the file to `src/common/CMakeLists.txt`.
  - Switch `src/common/rslConstants.cpp:21` from `ri.h` to `"common/riVocabulary.h"`.
  - Remove `EXCLUDE_FROM_ALL` from T037's translation unit.
- [ ] T041 [US1] Switch the 10 `ri.h` includers outside Scene Interface to `"common/riVocabulary.h"`:
  - `src/libshader/shading/{rendererServices.h,shader.h,shaderPl.h}`;
  - `src/ri/geometry/{object.cpp,subdivisionCreator.cpp}`;
  - `src/ri/hiders/reyes.cpp`;
  - `src/ri/state/{attributes.cpp,options.h}`;
  - `src/ri/texture/{texmake.cpp,texmake.h}`.

  If any needs a symbol `riVocabulary.h` doesn't declare (an `Ri*` function), stop and report it:
  R0 says none should.
- [ ] T042 [US1] Move the variable grammar: `git mv src/ri/parse/{variable.h,variable.y,variable.l} src/common/`.
  - In `src/common/CMakeLists.txt`, add `flex_file(variable.l lex.var.cpp var)` and
    `bison_file(variable.y variable_generated.cpp var)`, and add the generated sources to the
    OBJECT library.
  - In `src/ri/CMakeLists.txt`, delete `:154`, `:160`, the `variable_generated.cpp` entry at `:164`
    and the ribOut whitelist entry at `:505`.
  - In `variable.y`, drop the unused `renderer.h`/`error.h` includes.
  - Move `#define VARIABLE_NAME_LENGTH 64` from `src/ri/core/ri_config.h` into `variable.h`, and
    drop `variable.h`'s `ri_config.h` include.
  - Symbol hygiene in `variable.l`:
    - make `boolwrap`, `found_mot` and `buf_state` `static` if they are only used in that file;
    - rename `init_lexical_parser`/`close_lexical_parser` to `varInitLexer`/`varCloseLexer` if
      `variable.y` calls them, or make them `static` otherwise.
- [ ] T043 [US1] Switch the 9 `variable.h` includers to `"common/variable.h"`:
  - `shader.h`, `shaderPl.cpp`, `pl.h`, `blobby.cpp`, `displayChannel.h`, `userAttributes.h`,
    `rendererFiles.cpp`, …
  - Find them with `grep -rln '"variable.h"'`.
- [ ] T044 [US1] Update `tests/architecture/domain-map.txt`:
  - delete the `@build/src/ri/variable_generated.cpp` and `@build/src/ri/lex.var.cpp` lines;
  - add `prefix  @build/src/common/  Infra`.
- [ ] T045 [US1] Build, then:
  - `ctest -L common`: green, with `VariableDeclarations` still matching the **unchanged** golden
    file, plus `VocabularyConsistency` and `StateOwnership` for every token;
  - `ctest -L architecture`: prune the stale lines (the `ri.h`, `rendererc.h` and `variable.h`
    includes, the 87 token links and `parseVariable`), then re-run until green;
  - the full suite, unchanged;
  - `git diff master -- src/ri/parse/ri.h` must be empty.

**Checkpoint (PAUSE 5)**:
- the vocabulary and variables come from common;
- `ri.h` is byte-identical;
- the suite is unchanged.

Stage, summarize, and wait for review and commit.

---

## Phase 6: User Story 1 — `pl` belongs to Graphics State (Priority: P1)

**Goal**: move `pl.{h,cpp}` to `src/ri/state/` and invert `retrieveVariable` through the
Graphics State lookup hook (research R8, contracts/variable-lookup.md §1).

**Independent test**: the hook test passes, and `ctest -L architecture` reports every remaining
DB-016 line stale.

### Tests first

- [ ] T046 [US1] Write `Common_VariableLookupHook` (`tests/unit/common/test_variable_lookup.cpp`,
  `EXCLUDE_FROM_ALL` until T048). It links `ribRender openrendercommon`.
  - With nothing installed, `lookupVariable("P") == nullptr`.
  - Install a fake lookup that returns a static `CVariable` for `"foo"`; `lookupVariable("foo")`
    returns it and `lookupVariable("bar")` returns `nullptr`.
  - `installVariableLookup(nullptr)` clears it.
  - After `RiBegin(RI_NULL)`, `lookupVariable("P")` is non-null: the renderer core installed the
    real lookup. After `RiEnd()`, it is `nullptr` again.

**Checkpoint (TEST-APPROVAL 6a)**: T046 is red (it doesn't compile yet). Stage, and wait for approval
before T047.

### Implementation

- [ ] T047 [US1] Create `src/ri/state/variableLookup.{h,cpp}` exactly as contracts/variable-lookup.md §1
  specifies, and add `variableLookup.cpp` to `ri_sources` and to the ribVector source list in
  `src/ri/CMakeLists.txt`.
- [ ] T048 [US1] Move `pl` with `git mv src/ri/core/pl.h src/ri/core/pl.cpp src/ri/state/`.
  - Update `src/ri/CMakeLists.txt` (`:99`, plus the ribVector list if it names `core/pl.cpp`).
  - In `pl.cpp`:
    - remove `#include "renderer.h"`;
    - replace the two `CRenderer::retrieveVariable(...)` calls (`:1232`, `:1242`) with
      `lookupVariable(...)`;
    - include `"state/variableLookup.h"` in the same style as its other state includes.
  - Update includers of `pl.h`: the 11 Geometry headers/sources, `rendererContext.cpp`,
    `renderer.cpp`, `rendererStatics.cpp` and `preview/libribpreview/ribGeometryContext.cpp`. Follow
    each file's existing include style for state headers.
  - Remove `EXCLUDE_FROM_ALL` from T046.
- [ ] T049 [US1] In `src/ri/render/rendererDeclarations.cpp`, call
  `installVariableLookup(&CRenderer::retrieveVariable)` at the end of `CRenderer::initDeclarations()`
  and `installVariableLookup(nullptr)` at the start of `CRenderer::shutdownDeclarations()`.
- [ ] T050 [US1] Build, then:
  - `ctest -L common`, all green;
  - `ctest -L architecture`: prune the stale `pl` lines;
  - `grep -c '^DB-016' tests/architecture/allowlist.txt` must print `0`; also delete the empty
    `# --- DB-016 …` section header;
  - lower the metric (Infra → 0/0; Graphics State absent or 0);
  - the full suite, unchanged.

**Checkpoint (PAUSE 6)**:
- zero DB-016 lines remain;
- the metric is lowered;
- the suite is unchanged.

Stage, summarize, and wait for review and commit.

---

## Phase 7: User Story 4 — Existing programs and embedders keep working (Priority: P2)

**Goal**: prove binary, source and package compatibility (FR-014, SC-006) and the US2 binding
result on macOS (SC-002).

**Independent test**: quickstart §3 and §5 pass on macOS.

- [ ] T051 [US4] Install the new libraries in place of the old ones, and run unchanged consumers
  against them (spec US4 scenario 2).
  - **Install the new tree over the in-place root** that T004 created and built against:
    `DESTDIR=$S/023-inplace nice -n 19 cmake --install build`, keeping T002's `/usr/local`
    snapshot and diff.
  - Run T004's **unchanged** binaries, built before any source change, with no loader variables:
    `$S/023-compat/run.sh $S/023-inplace new`.
  - Compare each `out-<lib>-base` with `out-<lib>-new` using the method T004a established:
    `$S/023-compat/compare.sh $S/023-compat base new`.
  - Also install fresh to `$S/023-new` (same `/usr/local` check), and run
    `render-installed.sh $S/023-new`, which must render.
- [ ] T052 [P] [US4] Check the re-export and source compatibility.
  - `otool -l` of all three RI libraries shows `LC_REEXPORT_DYLIB … libRIBCommon`.
  - `git diff --quiet master -- src/ri/parse/ri.h`.
  - `build-package.sh $S/023-new $S/023-pkg-new`: both the Shared and Static targets build and
    render.
  - `build/orender-wire.app/Contents/Frameworks` contains `libRIBCommon`.
  - The installed RI libraries can find `libRIBCommon` when loaded by absolute path.
    - Inspect `otool -l $S/023-new/<prefix>/lib/libRIB{Render,Vector,Out}.1.dylib | grep -A2 LC_RPATH`,
      or `readelf -d … | grep RUNPATH` on Linux.
    - From a scratch program run with no loader variables, `dlopen` each library by absolute path;
      each must load and resolve `libRIBCommon`.
- [ ] T053 [P] [US2] Check the shading binding (quickstart §3): diff the
  `dynamically looked up` list against `tools/libshader-nm-m-before-macos.txt` (29 entries). Only removals are
  allowed, and the removals must be exactly the infrastructure symbols.
- [ ] T054 [US4] Run the full suite, including the python/lua binding tests. Confirm
  `grep -c '^DB-001' tests/architecture/allowlist.txt` is ≤ 18, and that the allowlist diff
  against master has no added line.

  Record T051–T054 in `validation.md`.

**Checkpoint (PAUSE 7)**: compatibility is proven on macOS. Stage, summarize, and wait for review and
commit.

---

## Phase 8: Polish & Cross-Cutting Concerns

- [ ] T055 [P] Write `specs/023-common-infrastructure-floor/tools/perf_ab.sh` (research R12).
  - Arguments: `<baseline-install-root> <build-dir> <rounds>`.
  - It covers the 12 RIBs `examples/rib/tests/sphere-{cfrom,ctransform,matrixops,comparisonlogic,arrayops,gather}-reyes{,-slo}.rib`.
  - Runs are serial and interleaved: baseline then new, each round, with the per-side library,
    shader and display pinning from T003.
  - It reports the median wall time per RIB and side and the ratio, and exits non-zero if any ratio
    is above 1.02.
- [ ] T056 Run `nice -n 19 tools/perf_ab.sh $S/023-base build 7` on an idle machine and record the
  table in `validation.md` (SC-007).
  - If a ratio is above 1.02, rerun once to rule out noise.
  - If it is still above, stop and report with per-RIB data; don't tune silently.
- [ ] T057 [P] Update `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` (research R14).
  - Move DB-016 under `## Resolved`, with "resolved by spec 023, commit and date recorded at
    merge" for now.
  - Correct its Direction text, which says "forwarding headers for one spec".
  - Update DB-001's Evidence and Size to the remaining lines (≤ 18), noting that the 024 brief's
    `parseVariable` service is no longer needed.
  - Update the sizes of any other entry whose allowlist count changed; compare per-entry counts
    against master.
  - In DB-016's Resolved entry, add "Follow-up: the shared `globalArena`/`stats` become injected
    per-context state in specs 029 and 039 (constitution rule 1)."
- [ ] T058 [P] Add a *Known gotchas* item to `CLAUDE.md` covering four rules:
  - `libRIBCommon` is the only definer of process-wide state;
  - `ribRender_static` must link the shared `libRIBCommon`;
  - the RI libraries re-export it;
  - `ri.h` keeps duplicate token declarations, guarded by `Common_VocabularyConsistency`.

  Also update the Repository layout notes for `src/common`, and add `ctest -L common` to Testing.
- [ ] T059 [P] Update the documentation.
  - `DEVNOTES.md`: add a status-table entry for spec 023.
  - `INSTALL.md` and `COMPILING.txt`: the new shared `libRIBCommon`, and the Static package
    component now requiring the shared targets.
- [ ] T060 Prepare the Linux validation package in `validation.md` (research R13), as for spec 022:
  - an rsync block to `juvenal@ubuntu-2404-desktop.v2-labs.io:/home/juvenal/Projects/Development/CLI/openrender`;
  - a sha256 manifest of the changed files;
  - a command block that configures a fresh `build-023` (GCC 13, `-Werror`) and builds at
    `nice -n 19 … -j4`;
  - then `ctest -L architecture`, `ctest -L common`, and the full suite, which should show the same 9
    pre-existing non-architecture failures as master;
  - the Linux checks from quickstart §3 (`readelf -d`, `nm -D`, `LD_DEBUG=bindings` against the
    T005 "before" files) and §5 (the in-place compat run with no loader variables, as in T051).

  Wait for the maintainer's results and record them. Linux green is a merge prerequisite.
- [ ] T060a Reduced-feature builds, in scratch build directories inside the worktree, deleted
  afterwards (spec Edge Cases, "Build options"):
  - `cmake -B build-nojit -S . -DOPENRENDER_ENABLE_JIT=OFF` and
    `cmake -B build-noexr -S . -DCMAKE_DISABLE_FIND_PACKAGE_OpenEXR=ON`;
  - for each: `nice -n 19 cmake --build <dir> --config Release -j4`, then
    `nice -n 19 ctest --test-dir <dir> -L "common|architecture" --output-on-failure -j4`.

  Record the results in `validation.md`.
- [ ] T061 Final pass on macOS:
  - `ctest -L architecture`, `ctest -L common`, and the full suite;
  - confirm the allowlist diff against master contains only deletions, and the metric diff only
    decreases.

  Update the checkboxes in this file.

**Checkpoint (PAUSE 8, final)**: the feature is complete and Linux is confirmed. Stage, summarize,
and wait for review and commit; then run `/speckit-converge`.

- [ ] T062 **After the maintainer merges**: in `DOMAIN_BOUNDARIES.md`, replace DB-016's "commit
  and date recorded at merge" with the merge commit and date, and post the closing comment on #29
  **only with the maintainer's approval**. Comments are outward-facing.

---

## Dependencies & Execution Order

### Phase dependencies

| Phase | Depends on |
|---|---|
| 1 Setup | nothing; must run before any source edit |
| 2 Foundational | 1; blocks everything after it |
| 3 US3 diagnostics | 2. Comes **before** Phase 4: `stats.cpp` calls `info()` and `fileResource.cpp` calls `error()`, and a shared `libRIBCommon` may not reference upward (R1) |
| 4 US2 core + arena | 3 |
| 5 US1 vocabulary + variables | 2; ordered after 4 to keep each pause reviewable. 5 and 4 touch disjoint files except the CMake source lists |
| 6 US1 `pl` | 4 (arena), 5 (`variable.h`, `rendererc.h` locations) |
| 7 US4 | 6 |
| 8 Polish | 7 |

### Within each phase

- Tests come first, then TEST-APPROVAL, then implementation, then the allowlist/metric prune and
  the review PAUSE.
- `git mv` tasks precede the include-switch tasks that depend on the new paths.

### Parallel opportunities

| Phase | Parallel tasks |
|---|---|
| 1 | T003, T005 |
| 2 | T011, T012, T013 (after T008–T010) |
| 3 | T015, T016, T017, T018 |
| 4 | T024, T025, T026; T031–T035 (disjoint domains) |
| 5 | none worth splitting: T038–T043 share include rewrites |
| 7 | T052, T053 |
| 8 | T055, T057, T058, T059 |

## Parallel example: Phase 4

```text
# tests first, together:
T024 tests/unit/common/test_common_arena.cpp
T025 tests/unit/common/shading_infra_binding.cmake
T026 tests/unit/common/owned-symbols.txt
# after T027–T030, the globalMemory rewrite by domain, together:
T031 src/ri/parse/*      T032 src/ri/texture/*      T033 src/ri/hiders/*
T034 src/libshader/*     T035 src/ri/render/*, core/pl.cpp, dataviewer, preview
```

## Implementation Strategy

**MVP is Phases 1–4**. Common is the single owner, diagnostics are context-free, and the shading
library gets its infrastructure from common. That is US2 complete and US3 complete, and it covers
most of US1's edges: the 45 `error.h` includes, the 39 diagnostic links and the 9 `globalMemory`
links.

The rest is incremental:
- Phase 5 removes the vocabulary and variable edges (117 lines).
- Phase 6 removes the last 5 `pl` lines and resolves DB-016.
- Phase 7 proves compatibility.
- Phase 8 measures performance, updates the docs and validates on Linux.

Expected DB-016 deletions per phase: 84 + 9 + 117 + 5 = 215.

Each phase leaves the tree green, with a shorter allowlist and a lower metric, so any pause is a
safe stopping point.
