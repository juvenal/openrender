# Validation record: spec 037 (versioned plug-in ABIs)

Filled phase by phase. Sections in the order tasks.md T001 gives. Numbers are measured, not copied from plan.md or research.md.

## 1. Preflight and baseline numbers (T001, T002, T003, T004, T005a)

**Preflight (T001)**

| Item | Value |
|---|---|
| Worktree | `/Volumes/Projects/Development/CLI/openrender-worktrees/037-plugin-abi-semver`, branch `037-plugin-abi-semver` |
| HEAD at work start | `05a00e4` (the tasks commit; tasks.md names `3a732d1`, the plan commit, which is its ancestor) |
| BASE | `486decd`; `git merge-base --is-ancestor 486decd HEAD` true; `git diff --stat 486decd HEAD -- . ':!specs' ':!.specify'` empty |
| Free space on `/Volumes/Projects` | 10 GiB at start (`df -h`), 9.3 GiB after the Phase 1 build and captures |
| `osLoadModule` sites | exactly the six of research M1: `src/ri/parse/ri.cpp:1762`, `src/ri/render/rendererDisplay.cpp:840`, `src/ri/render/rendererFiles.cpp:849`, `src/ri/render/rendererContext.cpp:5522`, `src/ri/geometry/implicitSurface.cpp:44`, `src/libshader/compiler/rslo.cpp:1704` |
| Project rules followed | `## Spec-kit agent rules` of `CLAUDE.md`; the worktree copy and the main checkout's copy hold the identical section (the main copy has staged changes, so it is the newer file) |

**Configure (T002)**: `cpujob 037-configure cmake -B build -S . -DCMAKE_BUILD_TYPE=Release`, plain.

| Cache value | Result |
|---|---|
| Generator | Unix Makefiles |
| Toolchain file | `/Users/juvenal/.vcpkg/scripts/buildsystems/vcpkg.cmake` (found through `VCPKG_ROOT`; `VCPKG_INSTALLED_DIR=/Users/juvenal/.cache/vcpkg-installed/openrender`) |
| `OPENRENDER_ENABLE_JIT` | ON |
| `INSTALL_SELFCONTAINED` | ON (the default; the Linux tree sets OFF) |
| `-Werror` | not a cache variable: `openrender_common_flags` (root `CMakeLists.txt`) carries `-Wall -Wextra -Werror`, and every target that links it or `openrendercommon` inherits it |
| `OR037_RECAPTURE` (T005a) | OFF after the reconfigure; no existing `OPENRENDER_*` option changed value |

**Build (T003)**: `cpujob 037-p1-build cmake --build build --config Release -j4`, clean under `-Werror`, exit 0.

**Baseline suite (T004)**, before any test was added: `ctest -LE determinism-sweep -j4`: **895 tests, 894 passed, 0 failed, 1 skipped** (`LibShader_Format_rslo_lower_minor`, skipped by its own condition). 234 s. `ctest -L architecture`: 21/21 passed. The sorted passing names are in `tools/baseline-pass-list.txt` (894 lines), the full archcheck report in `tools/archcheck-base-macos.txt`.

Archcheck on the starting tree: `ALLOWED DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-010 x 8 DB-011 x 173 DB-015 x 10 DB-017 x 78 DB-019 x 3 DB-021 x 3 DB-025 x 12`; `METRIC TOTAL refs 652 distinct 98`; `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`.

Failing baseline tests: none.

**Phase 2 build options (T046).** `OPENRENDER_TSAN` is a new opt-in option (OFF; when ON it adds `-fsanitize=thread` to every C and C++ compile and link and forces `OPENRENDER_ENABLE_JIT=OFF`). After the reconfigure the everyday macOS cache reads `OPENRENDER_TSAN=OFF`, `OPENRENDER_ENABLE_JIT=ON` (unchanged), `OR037_RECAPTURE=OFF`.

## 2. Red evidence (T030)

`cpujob ctest --test-dir build -L plugin_abi -j4 --output-on-failure`, run twice (and twice more as dedicated runs on the final code, `037-p1-abi3` and `037-p1-abi4`) with the same result: **18 tests, 8 passed, 10 failed**. The ten failures are the ten defect checks, each failing on the assertion about its own defect:

| FR | Test | Why it is red (from the test's own output) |
|---|---|---|
| FR-031 | `ShadeopAbi_SignatureMatches` | `static_assert` fails: `void (*)(void *, int, char **)` (the `SHADEOP` macro) is not `void (*)(void *, int, void **)` (`dsoExecFunction`). A control translation unit with the same flags compiles |
| FR-032 | `ProceduresAbi_BareNameLocated` | `Failed to load DLO "bare_name"`; exit status 255; the module's log is empty (no `dloInit`). The loader called `osLoadModule(name)` with the unresolved name |
| FR-033 | `ShadeopAbi_MissingExec` | no diagnostic names the unresolved exec symbol (the render prints nothing). See the note below: the other three assertions of T015 are green on the starting tree |
| FR-034 | `ProceduresAbi_DynamicLoadRelease` | log: `ConvertParameters ... nestedFree` (frame 1), `ConvertParameters ... nestedFree` (frame 2), then `unload`: the module is unmapped at process exit, not between the frames |
| FR-035 | `DisplayAbi_ReleaseAfterFinish` | log: `displayStart displayData displayFinish displayStart displayData displayFinish unload`: no unload between the frames |
| FR-036 | `PluginAbi_DocPaths` | stale: `#include "ri/dsply.h"` and `#include "file/file_base.h"` (`DISPLAY_PLUGIN_GUIDE.md`), `src/file/CMakeLists.txt` (`INSTALL_ARTIFACTS.md`), plus one the spec does not list: `src/framebuffer/` (`DISPLAY_PLUGIN_GUIDE.md`; the directory is `src/display/framebuffer/`) |
| FR-037 | `ShadeopAbi_ParserSingleDefiner` | `dsoParse` is defined in 4 of 4 libraries (`libRIBRender`, `libRIBVector`, `libRIBOut`, `libshader_compiler`) |
| FR-038 | `PluginAbi_InstallHeaders` | `file_base.h`: `#include "common/os.h"` names a header outside the installed set; compiling it alone: `fatal error: 'common/os.h' file not found`. The other six installed headers compile alone as C++20 |
| FR-039 | `DisplayV1_LateLookup` | all four late lookups (`quantize`, `dither`, `near`, `type`) return non-NULL |
| FR-040 | `DisplayAbi_UnsupportedParamType` | `RiDisplay` with `"double dd"` and `"float ff"` stores 2 parameters (expected 1) and prints no warning (expected 1). The program leaves with `_exit(1)` before it clones or frees the display (both read the uninitialized `type`) |

**FR-033 and the signal of T015.** tasks.md says the unresolved exec is called and asks for the signal (SIGSEGV or SIGBUS). Measured on the starting tree: **there is no signal and no call**. `dsoLoadCallback` tests `exec != NULL` on the address of the output slot (always true), so the first module whose table has the prototype ends the scan of *its own directory*, resolved or not; `getDSO`'s own test is on the value (null), so an unresolved exec makes `getDSO` return "not found". With A (no exec) and B (full operand) in two directories, B's module overwrites the slots and the operand is found; with A alone the shader fails with the generic `Failed to locate DSO function`. The three assertions T015 names (exit 0, `has_exec` ran, `missing_exec` did not) are therefore **green on the starting tree**. The red assertion is the one FR-033 states and the starting tree does not meet: *a diagnostic naming the symbol*. That is a change to T015 as written, made so the check can fail (SC-014); it is listed for approval at the T032 stop. A same-directory arrangement would be red on the first three assertions but depends on the file-system enumeration order (FR-013 fixes that only after Phase 4).

Characterization tests, green and stable on both runs: `DisplayAbi_PixelsIdentical`, `DisplayAbi_LegacyPixelsIdentical`, `ProceduresAbi_Scenes`, `ProceduresAbi_MissingSymbol`, `Texture3dChannels_Session`, `Texture3dChannels_NoSession`, `Texture3dChannels_Unknown`, `ShadeopAbi_DeclarationCharacterization`.

### Phase 2 red evidence (T034 to T038)

`cpujob 037-p2-red ctest --test-dir build -R 'PluginAbi_(Headers|VersionHistory)|ShadeopAbi_SignatureMatches|PluginAbi_InstallHeaders'`: **5 of 5 red**, for these reasons:

| Test | Why it is red |
|---|---|
| `PluginAbi_HeadersC17` | `orplugin.h` not found; `dlo.h`, `implicit.h`, `dsply.h` alone: `error: expected identifier or '('` (the unguarded `extern "C" {` at line 42, 43 and 52); the all-headers translation units fail the same way in both orders. `shadeop.h` alone compiles as C. A control translation unit with the same flags compiles |
| `PluginAbi_HeadersCxx20` | `orplugin.h` not found (alone, and in both all-headers translation units); the four family headers compile as C++20 |
| `ShadeopAbi_SignatureMatches` | both halves: the C++ `static_assert` (`void (*)(void *, int, char **)` against `void (*)(void *, int, void **)`), and the C half, `SHADEOP(f)` as strict C17: `error: expected identifier or '('` (the macro expands to `extern "C"`) |
| `PluginAbi_InstallHeaders` | the C++20 pass as in Phase 1 (`file_base.h`: `common/os.h` not in the set), and the new C17 pass: `dlo.h`, `implicit.h`, `dsply.h` do not compile alone as C17 (`expected identifier or '('`), `file_base.h` does not either (C++ class). `ri.h`, `ptcapi.h` and `shadeop.h` compile alone as strict C17 |
| `PluginAbi_VersionHistory` | none of `shadeop.h`, `dlo.h`, `implicit.h` defines `SHADEOP_ABI_VERSION`, `DLO_ABI_VERSION`, `OR_DYNAMICLOAD_ABI_VERSION` or `IMPLICIT_ABI_VERSION`; no history table |

Tests whose API does not exist yet, registered `EXCLUDE_FROM_ALL` + `DISABLED` (shown as `Not Run (Disabled)`): `PluginAbi_Convention`, `PluginAbi_VersionCheck`, `PluginAbi_SingleCheckRoutine` (the last until T093). Red state, from `cpujob cmake --build build --target ...`:

| Target | Compile error |
|---|---|
| `test_plugin_convention` | `conv_table.h:28:10: fatal error: 'orplugin.h' file not found` (the fixtures build first); the program itself: `test_convention.cpp:42:10: fatal error: 'common/orplugin.hpp' file not found` |
| `test_plugin_version_check` | `version_fake.c:32:10: fatal error: 'orplugin.h' file not found`; the program itself: `test_version_check.cpp:49:10: fatal error: 'common/orplugin.h' file not found` |

`ctest -L plugin_abi` after the Phase 2 tests: 24 registered, 21 run: **8 passed** (the Phase 1 characterization tests, unchanged), **13 failed** (the ten Phase 1 defect checks, whose reasons are unchanged, plus `PluginAbi_HeadersC17`, `PluginAbi_HeadersCxx20`, `PluginAbi_VersionHistory`), 3 disabled. `PluginAbi_VersionHistory` was also run by hand against scratch headers: green for correct ones, red for a newest row below the macro, and for a MINOR of 256.

### Phase 2 green evidence (T047, T048)

After T039 to T046 (`orplugin.h`, `orplugin.hpp`, `pluginAbi.{h,cpp}` in `libRIBCommon`, the three headers and `dsply.h` made C-includable with visibility, the public header list, `OPENRENDER_TSAN`):

| Test | Result |
|---|---|
| `ShadeopAbi_SignatureMatches` | green (C++ and C halves) |
| `PluginAbi_HeadersC17`, `PluginAbi_HeadersCxx20` | green |
| `PluginAbi_VersionCheck` | green (every row of data-model §2, raw garbage, cache replay, never retried) |
| `PluginAbi_Convention` | green |
| `PluginAbi_VersionHistory` | green (the three family headers declare their versions and histories) |
| `PluginAbi_InstallHeaders` | red, as expected: only `file_base.h` fails (C++20 and C17: `common/os.h` is not installed) until Phase 7; `orplugin.h`, `orplugin.hpp` and the six other headers pass in both languages |
| `PluginAbi_SingleCheckRoutine` | disabled until T093 |

Full macOS run (`ctest -LE determinism-sweep -j4`): 919 registered, **908 passed**; the failures are the nine Phase 1 defect checks not yet fixed (`ShadeopAbi_MissingExec`, `PluginAbi_DocPaths`, `DisplayAbi_ReleaseAfterFinish`, `DisplayV1_LateLookup`, `ShadeopAbi_ParserSingleDefiner`, `ProceduresAbi_BareNameLocated`, `ProceduresAbi_DynamicLoadRelease`, `DisplayAbi_UnsupportedParamType`, `PluginAbi_InstallHeaders`); plus the disabled `PluginAbi_SingleCheckRoutine` and the one skipped test of the baseline. Every one of the 894 baseline passes still passes, and the six turned-green tests are new passes. `ctest -L plugin_abi` twice as dedicated runs: 23 run, 14 passed, 9 failed (the same nine). Build: clean under `-Werror`; the 23 `warning:` lines are the linker's duplicate-library notes and the Swift package note, the same 23 as the Phase 1 build.

**Mutation checks (T047).** `SHADEOP` changed back to `char *argv[]`: `ShadeopAbi_SignatureMatches` red (the C++ `static_assert`). One byte of the C table of `conv_current` changed (`OR_ABI_VERSION(1, 0, 1)`): `PluginAbi_Convention` red (`the C initializer equals orplugin::makeTable<ConvPlugin> byte for byte`). Both reverted; the restored files equal the backups (`diff` empty). A restore within the same second as the mutated build left the mutated module in the build tree, and the next full run showed `PluginAbi_Convention` red until the file was touched and rebuilt: an artifact of the mutation, not of the code.

### Phase 3 red evidence (T049 to T054)

Fixtures added: the `legacy`, `v2`, `newer` and `hidden` variants of the dlo and implicit modules (the `good`, `bare_name` and `missing_symbol` ones exist), and the `versioned` and `v2` DynamicLoad generators (`plain` and `nested` now declare no version explicitly; `dynload_fixture.c` includes `dlo.h` for `OR_DYNAMICLOAD_DECLARE_VERSION`). Tests:

| Test | State | Why |
|---|---|---|
| `ProceduresAbi_Versions` | red, enabled | in-process, real RIB statements via `RiReadArchive`: `newer`, `legacy` and `v2` produce no `CODE_VERSION` diagnostic (expected one), and the MAJOR 2 modules are loaded and **called** (`v2:dloInit`, `v2:implicitInit`, `v2:ConvertParameters` in their logs), for dlo, implicit and DynamicLoad alike. The `good`, `hidden`, `plain` and `versioned` cases already pass |
| `ProceduresAbi_SameRoutine` | red, enabled | no diagnostic exists to compare: "exactly one diagnostic" fails for each family and kind |
| `ProceduresAbi_NoLoadOutsideModule` | red, enabled, until Phase 5 | six files outside `proceduralModules.cpp`, `shadeopModule.cpp` and `displayHost.cpp` call `osLoadModule` (`ri.cpp`, `rendererDisplay.cpp`, `rendererFiles.cpp`, `rendererContext.cpp`, `implicitSurface.cpp`, `rslo.cpp`) |
| `ProceduresAbi_ConcurrentAcquire`, `ProceduresAbi_ObjectAndFrames` | `EXCLUDE_FROM_ALL` + `DISABLED` until T061 | compile error of both programs: `fatal error: 'procedures/proceduralModules.h' file not found` |
| `PluginAbi_LoadMissingModule` | enabled; **green on macOS, red (aborts) on Linux** | added with T055a (maintainer's decision at checkpoint 2): `osLoadModule` of a bare name that exists nowhere, a relative path that does not exist and a file that is not a module must return `NULL` |

`ctest -L plugin_abi` on macOS after the Phase 3 tests: 30 registered, 27 run: **15 passed, 12 failed** (the nine Phase 1/2 reds, plus `ProceduresAbi_Versions`, `ProceduresAbi_SameRoutine`, `ProceduresAbi_NoLoadOutsideModule`), 3 disabled. On Linux (`openrender-037`, re-synced and checksum-verified: 3184 files OK; build clean under GCC 13 `-Werror`, 0 warnings): 14 passed, **13 failed**: the same twelve plus `PluginAbi_LoadMissingModule`, which ends in `*** buffer overflow detected ***: terminated` (`Subprocess aborted`); `ProceduresAbi_Versions` and `_SameRoutine` fail on the same assertions as on macOS.

Added to Phase 3 by the maintainer's decision at checkpoint 2: task **T055a**, the `osLoadModule` realpath fix (`realpath(name, nullptr)` + `free`), with `PluginAbi_LoadMissingModule` and the clean Linux failure of `ProceduresAbi_BareNameLocated` as its evidence. The `public_include` build-tree mechanism of Phase 2 was approved as built.

### Phase 3 green evidence (T055 to T061b, T062)

After T055 to T060 (`dlo.h`/`implicit.h` moved to `src/procedures/`, `proceduralModules.{h,cpp}` with the `openrenderprocedures_obj` objects in `ribRender`/`ribRender_static`, the `procDynamicLoadHook.h` hook in the Scene Interface and `rendererProcedures.cpp` in the core, the DLO and implicit branches and classes on the module API, `releaseFrameProceduralModules()` right after `root->destroy()`) and T055a (`osLoadModule` realpath fix, the maintainer's decision at checkpoint 2):

| Test | Result |
|---|---|
| `ProceduresAbi_Versions`, `ProceduresAbi_SameRoutine`, `ProceduresAbi_BareNameLocated` (FR-032), `ProceduresAbi_DynamicLoadRelease` (FR-034), `ProceduresAbi_ConcurrentAcquire`, `ProceduresAbi_ObjectAndFrames` | green |
| `PluginAbi_LoadMissingModule` | green on macOS and, after T055a, on Linux |
| `ProceduresAbi_MissingSymbol` (texts unchanged), `ProceduresAbi_Scenes` (pixels unchanged) | green |
| `ProceduresAbi_NoLoadOutsideModule` | red until Phase 5: three other `osLoadModule` sites remain (`rendererDisplay.cpp`, `rendererFiles.cpp`, `rslo.cpp`) |

One change to my own code at this step, not to a test: the unversioned warning of `pluginAbi.cpp` no longer names the version symbol (`orDloAbiVersion`, `orImplicitAbiVersion`), because the approved `ProceduresAbi_SameRoutine` requires the three families' messages to be identical once path and interface name are replaced.

**Mutations (T061).** The `location` argument of `acquireDloModule` changed back to `name`: `ProceduresAbi_BareNameLocated` red (`Failed to load DLO "bare_name"`, no `dloInit`), as on the starting tree. `releaseFrameProceduralModules()` moved before `root->destroy()`: **`ProceduresAbi_DynamicLoadRelease` and `ProceduresAbi_ObjectAndFrames` stay green**, because under the REYES hider the nested procedural is consumed (and its `nestedFree` runs) during rendering, before the frame ends. Shown by hand instead, with a scratch RIB (`two_frames.rib` plus `Hider "raytrace"`, where the delayed objects survive until `root->destroy()`): with the mutation orender ends in a **segmentation fault** and the log stops at `nested:unload` with `nested:nestedFree` never logged (the free would run after the unmap); with the correct order each frame logs `nestedSubdivide`, `nestedFree`, `unload` and exits 0. The maintainer approved a raytrace variant at checkpoint 3: **`ProceduresAbi_DynamicLoadReleaseRaytrace`** (task T061c), the same two frames under `Hider "raytrace"`. Green without the mutation; with the release moved before `root->destroy()` it is red (`orender exited with 'Segmentation fault'`, one expansion logged instead of two), `ProceduresAbi_DynamicLoadRelease` and `_ObjectAndFrames` stay green as before; mutation reverted (the file equals its backup, no `MUTATION` text left), full `ProceduresAbi_` set green again except `ProceduresAbi_NoLoadOutsideModule` (Phase 5). Both mutations reverted; the files equal their backups.

**Added by the maintainer's decision, 2026-10-30 (include directories).** T061a: `src/includes/` renamed `src/include/` (root `CMakeLists.txt`, `domain-map.txt`, `INSTALL.md`, `CXX20_MIGRATION.md`; eleven includers named it with a path, `"includes/logging.hpp"`, and now say `"include/logging.hpp"`: `imager.h`, `rendererContext.cpp`, `rendererDisplay.cpp`, `rendererFiles.cpp`, `renderer.cpp`, `rendererStatics.cpp`, `shading.cpp`, `execute.cpp`, `rslOps.cpp`, `test_common_diagnostics.cpp`, `test_imager_guard.cpp`; the others used the bare name). T061b: `orplugin.h` and `orplugin.hpp` moved to `src/include/`, the build-tree `public_include` copy removed. The install-location change (self-contained `<ORENDERHOME>/include`, FHS `<prefix>/include/openRender/`, the package config following) is T102a, T105a and T107a in Phase 7, with the spec, plan, research and contract text updated now.

Full macOS run (`ctest -LE determinism-sweep -j4`): 925 registered, **915 passed**; every test of the Phase 2 pass list still passes (the visual, shading-parity, CSG, determinism, framebuffer and texture suites included), and the seven Phase 3 tests above are new passes. The eight remaining failures are the Phase 1 defect checks not yet fixed (`ShadeopAbi_MissingExec`, `PluginAbi_DocPaths`, `ProceduresAbi_NoLoadOutsideModule`, `ShadeopAbi_ParserSingleDefiner`, `DisplayV1_LateLookup`, `DisplayAbi_ReleaseAfterFinish`, `PluginAbi_InstallHeaders`, `DisplayAbi_UnsupportedParamType`); `PluginAbi_SingleCheckRoutine` is disabled.

Architecture (CP-c): 21/21; `ALLOWED` and `METRIC` lines identical to the base report, `RESULT PASS`, no `unclassified:`, `STALE` or `VIOLATION`. The domain map shows `src/procedures/` as `Procedures` and `src/include/` as `Infra`. Edges added (all allowed): include edges 1534 -> 1559 and link edges 893 -> 902 (Geometry -> Procedures, Core -> Procedures and Scene, Procedures -> Infra, the family headers -> `orplugin.h`).

### Phase 4 red evidence (T063 to T065)

Fixtures (`tests/plugins/shadeop/`): `shadeop_fixture.cpp` now builds the variants `good` (current interface, 1.1 per-thread table `probe_add_shadeop_threads[]` when the header has one), `hidden` (hidden visibility), `unaware` (declares 1.0.0, no per-thread table), `legacy` (no version symbol, a second table `probe_mul`), `newer` (1.9.0), `v2` (2.0.0) and `threads_null` (threadInit returns NULL for thread 3), besides `missing_exec` and `has_exec`; every `init`/`exec`/`cleanup`/`threadInit`/`threadCleanup` line carries the pointer it received or returned. `order_fixture.cpp` builds `alpha`, `zeta` (directory `order/first/`) and `beta` (`order/second/`), which define `probe_order(float)` and differ in what it returns. `plugin_shadeop_example` builds `examples/dsotest/dsotest.cpp` as a module. Shader `probe_two.sl` calls both operands of `legacy`. The fixture source compiles against the 1.0 header (no thread table) and the 1.1 one.

| Test | State | Why |
|---|---|---|
| `ShadeopAbi_Versions_Render` | red, enabled | seven failures on two-frame, four-thread `.rslo` renders: `good` and `hidden`: the per-thread init is never called; `newer`, `v2 then good`, `legacy`: no `CODE_VERSION` diagnostic (expected one); `v2` alone: no diagnostic and **194 entry point calls of the rejected module** (SC-001) |
| `ShadeopAbi_LegacyShared` | green by design, enabled | `unaware` on four threads: 192 `exec` calls on 4 threads, one data pointer, the image equals the one-thread image (decoded samples) |
| `ShadeopAbi_ExampleBuilds` | red, enabled | the example module does not export `orShadeopAbiVersion`; its README mentions neither `SHADEOP_DECLARE_VERSION` nor the concurrency rule |
| `ShadeopAbi_MissingExec` (Phase 1), `ShadeopAbi_ParserSingleDefiner` (Phase 1) | red as before | the diagnostic naming the symbol; `dsoParse` in four libraries |
| `ShadeopAbi_DeclarationCharacterization`, `ShadeopAbi_SignatureMatches` | green | unchanged |
| `ShadeopAbi_Versions_Registry`, `_PerThread`, `_ThreadLifetime`, `_ScanOrder`, `_CompilerAndRenderer` (one program, `test_shadeop_registry`) and `ShadeopAbi_ParserReentrant` (`test_shadeop_parser_reentrant`) | `EXCLUDE_FROM_ALL` + `DISABLED` until T068/T072 | compile error of both programs: `fatal error: 'shadeop/shadeopDeclaration.h' file not found` (`cmake --build build --target test_shadeop_registry test_shadeop_parser_reentrant`) |
| `ShadeopAbi_ParserNewSymbol` | `DISABLED` until T069 | the T019 script (new mode `only`) over `libRIBShadeop` and the four old libraries: with the library absent it ends `PRECONDITION: library ... libRIBShadeop-not-built does not exist` |

The two test programs were also compiled with `-fsyntax-only -Wall -Wextra -Werror` against a stub of the contract's declarations (scratch, not in the tree): they are clean. `ShadeopAbi_ParserReentrant` was built once against the **old** `dsoParse` (scratch shim, removed again): the process ends in a segmentation fault, so the test does catch a parser with static state; its real red state is the missing header.

`ctest -L plugin_abi` on macOS: 8 disabled, the Phase 1 to 3 reds unchanged (`PluginAbi_DocPaths`, `PluginAbi_InstallHeaders`, `ProceduresAbi_NoLoadOutsideModule`, `ShadeopAbi_MissingExec`, `ShadeopAbi_ParserSingleDefiner`, `DisplayV1_LateLookup`, `DisplayAbi_ReleaseAfterFinish`, `DisplayAbi_UnsupportedParamType`) plus the two new reds `ShadeopAbi_Versions_Render` and `ShadeopAbi_ExampleBuilds`; architecture 21/21.

Linux (`openrender-037`, re-synced and checksum-verified: 3200 files OK; build clean under GCC 13 `-Werror`, 0 warnings, the example module included): `ctest -L 'plugin_abi|architecture'` gives the same table: the two new reds (`ShadeopAbi_Versions_Render` with the same seven failures, 195 calls of the rejected module instead of 194; `ShadeopAbi_ExampleBuilds` with the same three), `ShadeopAbi_LegacyShared` green, eight disabled, the Phase 1 to 3 reds unchanged, architecture green.

**Phase 4 test approval (2026-10-31), the maintainer's decisions:** one diagnostic per operand for an out-of-range thread index (process-wide data returned); search-path entries carry a trailing separator; concurrent first calls on one index stay untested; the three extras accepted (`MODE=only`, the README assertions, the render-level lifetime check); `ShadeopAbi_DeclarationCharacterization` re-pointed at the new parser at T068 and `ShadeopAbi_ParserSingleDefiner` moved to `MODE=none` at T076 (both edits to approved tests approved); the "not osEnumerate order" half of `ShadeopAbi_ScanOrder` covered by a mutation at T076.

### Phase 4 green evidence (T066 to T076)

Implemented: `src/shadeop/` (`shadeop.h` moved with `git mv` and bumped to 1.1.0, `shadeopDeclaration.{h,cpp}`, `shadeopModule.{h,cpp}`, `CMakeLists.txt` building `libRIBShadeop` shared + static, exported and installed); the shading side (`CDSO` holds a `CShadeopOperand *`, `DSOEXEC_PRE`/`DSOVOIDEXEC_PRE` read `shadeopOperandData(operand, thread)` and `shadeopOperandExec`, `libshader_shading` links `openrendershadeop`); the renderer side (`CRenderer::shadeops` replaces `CRenderer::dsos`, created in `initFiles`, `beginFrame(numThreads)` called from `CRenderer::beginFrame` right after the thread count is final, deleted in `shutdownFiles`; `getDSO` builds the search path from `proceduralPath`); the compiler side (`CScriptContext::enumerateDso` calls `enumerateShadeopDeclarations`; `libshader_compiler` links the library); `dso.{l,y,h}` and their `flex_file`/`bison_file`/`dso_generated.cpp` lines deleted (ribVector and ribOut still link); the three DB-019 allowlist lines and the empty section deleted, `domain-map.txt` has `prefix src/shadeop/ ShaderDSO` and lost the `dso_generated`/`lex.dso` rules and the `shadeop.h` file rule; `examples/dsotest` declares `SHADEOP_DECLARE_VERSION()` and includes only `shadeop.h`, its README states the version and the concurrency rule.

Implementation decisions to review:

* **The `CDSO` handle.** `CDSO` stays an aggregate (`tests/shading_parity/recording_services.h` makes stub ones with `= {}`) holding `operand`, `name`, `prototype`. The registry owns it as the operand's *host object* (`setShadeopOperandHostObject`, destroyed after the operand's cleanup, before the unload): this is how `CRenderer::dsos` could go away entirely, as T071 says.
* **Init blocks.** `CRendererContext::init` (render/init.cpp) runs a shader's parameter-default code once at bind time, outside any shading context. A DSO call in it uses thread index 0 (`DSO_THREAD_INDEX` in `scriptFunctions.h`, defined before the first inclusion), and every operand has at least one slot, so such a call before the first frame works without a diagnostic.
* **An unresolved `exec` is a `warning()`** (`CODE_BADFILE`, once per module and symbol), because `orender` exits 255 after any `error()` and `ShadeopAbi_MissingExec` expects exit 0.
* **Search path entries.** The module appends a separator to an entry that lacks one (`oshader -I dir` passes bare directories); the renderer's entries already carry it.
* **Failed loads.** The renderer's scan keeps the old `Failed to load DSO` error for a file that does not load; the compiler's scan stays silent about it, as before.
* A module is loaded for its table once per scan and, when selected, kept loaded (its own handle) until the registry is released; the process-wide `init` runs at acquisition as `getDSO` always did.

Results on macOS (`cpujob` -j4; build clean under `-Werror`, 0 first-party warnings):

| Test | Result |
|---|---|
| `ShadeopAbi_DeclarationCharacterization` (re-pointed at `parseShadeopDeclaration`; `declarations.txt` byte-identical, `git diff` empty), `ShadeopAbi_SignatureMatches`, `ShadeopAbi_MissingExec` (the diagnostic names `probe_add`, exit 0), `ShadeopAbi_ParserSingleDefiner` (`MODE=none`, five libraries), `ShadeopAbi_ParserNewSymbol`, `ShadeopAbi_ParserReentrant`, `ShadeopAbi_PerThread`, `ShadeopAbi_ThreadLifetime`, `ShadeopAbi_ScanOrder`, `ShadeopAbi_CompilerAndRenderer`, `ShadeopAbi_LegacyShared`, `ShadeopAbi_ExampleBuilds` | green |
| `ShadeopAbi_Versions_Render`, `ShadeopAbi_Versions_Registry` | **red, one case each, a defect in the approved test** (task T076a): see below. Every other assertion of both tests passes: good and hidden (per-thread init once per index over two frames, last exec < threadCleanup < cleanup < unload), newer (one warning), v2 (one error naming file, interface, both versions and the remedy; none of its entry points entered), legacy (one warning across two operands) |
| `ProceduresAbi_NoLoadOutsideModule` | red until Phase 5: one `osLoadModule` site left outside the loading modules (`rendererDisplay.cpp`); `rendererFiles.cpp` and `rslo.cpp` are done |

The three defects in the approved tests (nothing applied; proposed fixes in T076a): (1) `Versions_Render` case `v2_then_good` expects `orender` to exit 0, but the rejection is an `error()` and orender exits 255 after any `error()`. (2) `Versions_Registry` case `v2 then good` expects one diagnostic, but the rejection of `v2.dsply` was reported by the previous case of the same process and a rejection is never reported twice. (3) Linux only: `Versions_Render` `good` and `hidden` compare `pthread_self()` of the `threadInit` thread with that of the `exec` thread, but frame 2 starts new threads, glibc reuses their ids, and a reused id can carry another thread index (measured on Linux: 4 ids, each with two data pointers; the implementation hands each thread index its own data). macOS passes by luck of id reuse.

Full macOS run (`ctest -LE determinism-sweep -j4`): 935 registered, **927 passed**; every test of the Phase 3 pass list (`build-037-snapshots/pass-list-p3b.txt`, 915 tests) still passes. The eight failures: `PluginAbi_DocPaths`, `PluginAbi_InstallHeaders`, `ProceduresAbi_NoLoadOutsideModule`, `DisplayAbi_ReleaseAfterFinish`, `DisplayV1_LateLookup`, `DisplayAbi_UnsupportedParamType` (Phase 5 to 7) and the two above; `PluginAbi_SingleCheckRoutine` is disabled.

**Mutations (T076)**, each reverted afterwards (the source equals its backup):

| Mutation | Result |
|---|---|
| test the address of an array of `exec` slots instead of the resolved `exec[0]` | `ShadeopAbi_MissingExec` red: `orender crashed ('Segmentation fault')`, the full operand never ran, no diagnostic naming `probe_add` |
| a file-static prototype string in `parseShadeopDeclaration` | `ShadeopAbi_ParserReentrant` red (SIGTRAP, memory corruption) three runs of three; the single-threaded `DeclarationCharacterization` stays green |
| every thread index through slot 0 | `ShadeopAbi_PerThread` red ("N threads get N distinct data pointers, got 1"), `ShadeopAbi_ThreadLifetime` red, and the thread check of `Versions_Render` red on macOS too |
| module file names in descending order | `ShadeopAbi_ScanOrder` red (compiler order and "the renderer picks module id 1, got 2") |
| no sort at all (the order `osEnumerate` returns) | `ShadeopAbi_ScanOrder` stays **green** on macOS: APFS returns names sorted, so the "not enumeration order" half cannot be shown by deleting the sort; the descending mutation shows the test does depend on the order |

Architecture (CP-c): 21/21 over a complete build; `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; `ALLOWED` loses `DB-019 x 3` and nothing else changes; `METRIC` lines identical; the allowlisted `Geometry -> Shading -> SceneInterface` cycle line (it went through `dso_generated.cpp -> dso.h`) is gone; objects 282 -> 280, edges 2461 -> 2463 (include 1559 -> 1551, link 902 -> 912: `Shading`, `Core` and the compiler to `ShaderDSO`, all allowed); no `unclassified:` (`src/shadeop/` is `ShaderDSO`). Report: `build-037-snapshots/archcheck-p4.txt`.

Linux (`openrender-037`, re-synced and checksum-verified: 3202 files OK; the removed `dso.{l,y,h}` and `src/ri/render/shadeop.h` deleted by hand; build clean under GCC 13 `-Werror`, 0 warnings): `ctest -L 'plugin_abi|architecture|shading_parity'` gives the same eight failures as macOS (six Phase 5 to 7 reds and the two `Versions_*` defects of T076a); everything else green, `ShadeopAbi_*` included. Full run (`ctest -LE determinism-sweep -j2`, before the clang-format-only touch-up): 937 registered, 921 passed, 16 failed = the seven known Linux failures of the baseline (`LibShader_McompConsistency`, `TextureTile_RenderByteIdentical`, four `TextureTile_Cache*`, one `CoverageAnalytic` t-junction test), the eight above, and `Visual_sphere-texture3d-bake3d-reyes`, which segfaulted once under `-j2`. That one is a race between the test and its `-slo` sibling, which write the same `017_texture3d_bake3d_probe.ptc` into the shared working directory (see the comment in `tests/visual/CMakeLists.txt`): alone it passed 6 of 6, and `-R texture3d -j2` failed once in 8 runs; texture3d code is not touched by this phase.

**T076a applied (maintainer approval, 2026-10-31).** The three fixes to the approved tests, exactly as proposed: `expect_exit_zero(v2_then_good)` removed (a comment says why); `ShadeopAbi_Versions_Registry` case `v2 then good` expects no further diagnostic; the per-thread `pthread_self` check moved from the two-frame `good`/`hidden` renders to new one-frame renders `good_f1`/`hidden_f1` (`render_1f.rib.in`), and the two-frame renders keep the once-per-index and end-of-life checks. Result: `ShadeopAbi_Versions_Render` and `ShadeopAbi_Versions_Registry` green on macOS and Linux. The one-frame check still detects the bug: with every thread index routed through slot 0 (mutation, reverted) `Versions_Render` fails with "exec ran on thread X with the data that thread Y initialized" for both `good (one frame)` and `hidden (one frame)`. The implementation decisions of the Phase 4 section above were accepted by the maintainer. `Visual_sphere-texture3d-bake3d-reyes` (shared `.ptc` file race with its `-slo` sibling) is known-flaky and is filed as a separate bug. Re-run after the three fixes: macOS `ctest -L 'plugin_abi|architecture|shading_parity'` 183 tests, 177 passed, the six failures are the Phase 5 to 7 reds (`PluginAbi_DocPaths`, `PluginAbi_InstallHeaders`, `ProceduresAbi_NoLoadOutsideModule`, `DisplayAbi_ReleaseAfterFinish`, `DisplayV1_LateLookup`, `DisplayAbi_UnsupportedParamType`); Linux (re-synced, 3203 files checksum-verified, build clean, 0 warnings) the same six, architecture green on both.

### Phase 5 red evidence (T078 to T082)

Fixtures (`tests/plugins/display/`): `v1_fixture.c` (hand-declared v1 signatures, builds on the starting tree) in the variants `v1_explicit` (1.0.0), `v1_newer` (1.4.0), `v3` (3.0.0), `v0` (MAJOR 0), `garbage` (`0xFFFFFFFF`), all three of the last also exporting `orDisplayPlugin`, `v1_hostile`, `v1_serial` and `v1_serial_fail` (the third `displayData` returns 0); `v2_fixture.c` (strict C17 against the new `dsply.h`, `EXCLUDE_FROM_ALL` until T085) in the variants `v2`, `v2_hidden`, `v2_newer` (2.9.0), `v2_record`, `v2_failfirst`, `v2_hostile`. `v1_legacy`, `v1_late_lookup` and `legacy_v1/` are the Phase 1 ones. The host interface the in-process tests drive is written into contracts/display-v2.md.

| Test | State | Why |
|---|---|---|
| `DisplayAbi_Versions_Reject` | red, enabled | `v3`, `v0`, `garbage`: no diagnostic (expected one naming the file, the version and the remedy), and each rejected module is **entered** (`displayStart`, `displayData`, `displayFinish` called as a v1 driver: 3 calls each) |
| `DisplayAbi_Versions_V1` | red, enabled | no legacy warning for `v1_legacy` across two displays, no newer-MINOR warning for `v1_newer` (`v1_explicit`: none, as expected) |
| `DisplayV1_LegacyLoads` | red, enabled | the frozen unversioned `file.dsply` renders the `tiff8` baseline pixel for pixel but prints **no** legacy warning (expected exactly one naming 1.2.0) |
| `DisplayV1_Explicit1x` | red, enabled | the behaviour (start, data, finish, no `displayRawData`) holds; the `v1_newer` warning is missing |
| `DisplayV1_HostileCopiesOnly` | red, enabled | `v1_hostile` writes through the pointers the lookup returns: they point into the renderer's statics and a string literal, and orender dies with a bus error before the normal display's `a.tif` is written |
| `DisplayV1_SerializedData` | red, enabled | 63 `displayData` entries while another call was inside; with a failing call 7 more, and the module unloaded (line 11) before the last call returned (line 17), or, on another run, orender segfaulting in the unloaded module |
| `DisplayV1_NoCastCheck` | red, enabled (in `checks/`) | 28 violations: `dsply.h` declares the v1 types, the in-tree drivers and `renderer.h` name them, `rendererDisplay.cpp` casts the custom display's RI callbacks and the resolved symbols to them, and neither the adapter files nor `displayHost.cpp` exist |
| `DisplayV1_CustomDisplay` | green by design, enabled | `RiCustomDisplay` from a program: start gets the name, the resolution, 4 samples and the mode; the callback answers `quantize`, `dither`, `near`, `far`, `gain`, `gamma`, `Nl`, `NP`, `screen`, `Software` and `type` ("custom") and `NULL` for an unknown name, a wrong count and a wrong type; the tiles add up to the image; one finish |
| `DisplayAbi_Versions_V2`, `_HiddenVisibility`, `_ZeroAndDepthOnly`, `_Descriptors`, `_TwoDisplayParams`, `_Snapshot`, `_LateDisable`, `_Hostile`, `_ReleaseAfterFinish_V2` | `DISABLED` until T085/T086 | the six v2 fixtures fail to compile: `unknown type name 'OrDisplayPlugin'`, `'OrDisplayContext'`, `'OrDisplayInfo'`, `use of undeclared identifier 'OR_DISPLAY_ABI_VERSION'` (`cmake --build build --target plugin_display_v2 ...`); `test_display_host.cpp` (five modes) fails with `fatal error: 'ri/render/displayHost.h' file not found` |
| `DisplayAbi_ReleaseAfterFinish`, `DisplayV1_LateLookup`, `DisplayAbi_UnsupportedParamType` | red as before | Phase 1 |
| `DisplayAbi_PixelsIdentical`, `DisplayAbi_LegacyPixelsIdentical` | green | unchanged |

The disabled programs were also checked against a scratch stub of the contract's declarations (`-fsyntax-only -Wall -Wextra -Werror`; the fixture in strict C17 `-Wpedantic`) and are clean. `ctest -L plugin_abi` on macOS: 69 tests, 13 failures (the six Phase 5 reds above, the three Phase 1 display reds, `PluginAbi_DocPaths`, `PluginAbi_InstallHeaders`, `ProceduresAbi_NoLoadOutsideModule`) and nine disabled; architecture 21/21. Linux (`openrender-037`, 3219 files checksum-verified, build clean under GCC 13 `-Werror`): the same 13 failures and nine disabled tests; the first Linux build failed on `usleep` in strict C17, fixed in the fixture with `nanosleep` and `_POSIX_C_SOURCE`.

**Phase 5 test approval (2026-11-01), the maintainer's decisions (T082a):** the host interface of contracts/display-v2.md (the core locates the module and passes its path in `SDisplaySpec::module`; a string parameter's data is the string itself); the host never calls `data` for a display with `numSamples == 0`; `stateConversions.h` asserts only `OrDisplayParamType` against `EDisplayParameterType`, `displayV1Adapter.cpp` asserts `ParameterTypeV1` against `OrDisplayParamType`, and `stateConversions.h`'s comment no longer names `ParameterType` (this replaces T085's wording); the scope of `v2_hostile`; the `DisplayV1_NoCastCheck` patterns (grep); the hostile test layout.

### Phase 5 green evidence (T083 to T094)

Implemented: **T083** the `RiDisplayV` case for `double`/`mpoint`/`quad` frees the copied name, warns once (`CODE_BADTOKEN`) and does not advance `j` (`DisplayAbi_UnsupportedParamType` green). **T085** `src/display/dsply.h` 2.0.0 (types, table, `orDisplayFindParameter`, history; the v1 types moved to `src/ri/render/displayV1Abi.h` with a `V1` suffix; `stateConversions.h` and `test_state_vocabulary.cpp` follow; `PluginAbi_VersionHistory` extended to `dsply.h`). **T086** `src/ri/render/displayHost.{h,cpp}` and `displayContext.h`. **T087** `displayV1Adapter.{h,cpp}`. **T088** `rendererDisplay.cpp`, `renderer.h` and the CMake lists: the file-static `currentDisplay`, the free function `findParameter`, the unload in `dispatch` and `displayKillMutex`'s unload use are gone (the mutex only guards the active count). **T088a/T089/T090/T091/T092** `CFileOutputBase` takes `const OrDisplayInfo *` and reads `quantize`, `dither`, `gamma` through `orDisplayFindParameter`; `file`, `openexr`, `rgbe` and `framebuffer` export `orDisplayPlugin` and declare 2.0.0; the framebuffer closes its connection to the `orender-fb` helper and frees its own display object in `finish` (the helper keeps running and its window stays open). The markers of the Phase 5 tests are removed (**T093**).

Implementation decisions to review:

* **The unbound-lookup log is once per process, not once per display.** The v1 callback carries no context, so a lookup with no bound display cannot say which display it belongs to; the adapter logs one warning for the process.
* **The module is released at `finishAll`, also for a display whose start failed** (data-model 4.2); a module that was rejected or lacks an entry point is unloaded at once, nothing of it having been entered.
* **Tables from the adapter are per display** (`displayV1ReleasePlugin` gives them back in `finishAll`), so a module shared by two displays gets two tables and its `dlopen` count is two.
* **A defect the hostile test found in my first host:** the renderer-provided parameter names pointed at string literals, and `v2_hostile` faulted writing to them. All names and values in the snapshot are host-owned copies now.
* **Unversioned fixtures** (`v1_hostile`) print the legacy warning, as any unversioned driver does.

Results on macOS (build clean under `-Werror`, 0 warnings): the full run (`ctest -LE determinism-sweep -j4`) has 953 registered, **948 passed**; every test of the Phase 3 pass list still passes, the visual, shading-parity, CSG, determinism, framebuffer and texture suites included, with `DisplayAbi_PixelsIdentical` and `DisplayAbi_LegacyPixelsIdentical` at zero differing samples (the PNG `gAMA` check included) and `State_VocabularyConsistency` green. Green that were red or disabled before: `ProceduresAbi_NoLoadOutsideModule` (the last `osLoadModule` site outside the loading modules is gone), `PluginAbi_SingleCheckRoutine`, `DisplayAbi_ReleaseAfterFinish`, `DisplayV1_LateLookup`, `DisplayAbi_UnsupportedParamType`, `DisplayAbi_Versions_Reject`, `_V1`, `_V2`, `_HiddenVisibility`, `_ZeroAndDepthOnly`, `_Descriptors`, `_TwoDisplayParams`, `_Snapshot`, `_LateDisable` (1000 cycles on 8 threads), `_ReleaseAfterFinish_V2`, `DisplayV1_LegacyLoads`, `_Explicit1x`, `_CustomDisplay`, `DisplayV1_NoCastCheck`, `PluginAbi_VersionHistory`. The five failures: `PluginAbi_DocPaths` and `PluginAbi_InstallHeaders` (Phase 7), and `DisplayAbi_Hostile`, `DisplayV1_HostileCopiesOnly` and `DisplayV1_SerializedData`, which fail on **defects in the approved tests themselves** (task T093a): the `failed` case of `SerializedData` requires at least 4 `displayData` calls where the contract allows exactly 3; the two hostile tests expect no diagnostic from `v1_hostile`, an unversioned fixture, which correctly prints the legacy warning. On scratch copies of the two scripts with those expectations corrected (not applied), `SerializedData` and both hostile modules pass.

**Mutations (T093)**, each reverted afterwards (the source equals its backup):

| Mutation | Result |
|---|---|
| unload the module in `dispatch` on a failed `data` | `DisplayAbi_LateDisable` red (segmentation fault: other threads are inside the unloaded driver) |
| a process-wide "current display" in the snapshot builder (the first display's quantizer is used for all) | `DisplayAbi_TwoDisplayParams` red (display B sees A's quantize and dither) and `DisplayAbi_Snapshot` red (three rows) |
| drop the adapter's per-display lock | `DisplayV1_SerializedData` red with a new failure: 63 calls entered while another was inside in the `ok` case (and 7 in the `failed` case) |

Architecture (CP-c): 21/21; `ALLOWED` loses nothing and gains nothing, `METRIC` lines identical, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; objects 280 -> 282 and edges 2463 -> 2480 (the host, the adapter and their headers under `src/ri/render/` (Core), the drivers' `orplugin.h` includes), no `unclassified:`. Report: `build-037-snapshots/archcheck-p5.txt`.

Linux (`openrender-037`, 3225 files checksum-verified; build clean under GCC 13 `-Werror`, 0 warnings): `ctest -L 'plugin_abi|architecture'` gives the same five failures as macOS and architecture green. Full run (`ctest -LE determinism-sweep -j2`): 954 registered, 942 passed, 12 failed = the seven known baseline failures (`LibShader_McompConsistency`, `TextureTile_RenderByteIdentical`, four `TextureTile_Cache*`, one `CoverageAnalytic` t-junction test), `PluginAbi_DocPaths`, `PluginAbi_InstallHeaders` and the three defects of T093a. With the proposed corrections on scratch copies of the two scripts, `DisplayV1_SerializedData` and both hostile modules pass on Linux too.

### Phase 6 evidence (T096 to T100)

Implemented: `src/ri/texture/texture3dChannels.{h,cpp}` (`STexture3dChannelInfo`, `CTexture3dChannelSource::find`, `setTexture3dChannelSource`, `texture3dChannelSource`; the `.cpp` is in `ri_sources`, so `ribRender` and `ribVector`, which both compile `texture3d.cpp`, have it); `CTexture3d::defineChannels(const char *)` asks the installed source and gives today's `Unknown display channel` error when there is none or it answers false (`texture3d.cpp` no longer includes `renderer.h` or `displayChannel.h`; it includes `common/variable.h` for the ptcapi form, which `renderer.h` used to bring in); the core's `CRendererChannelSource` in `rendererShadingServices.cpp`, backed by `channelRegistry()->retrieve(name)`, `variable ? variable->type : TYPE_FLOAT`, `fill` aliased to the registry's array, installed in `rendererShadingServicesInit()` (called from `beginRenderer`) and removed in `rendererShadingServicesShutdown()` (called from `endRenderer`); `CRenderer::retrieveDisplayChannel` and its declaration deleted; the three DB-021 allowlist lines and the section deleted; `metrics-baseline.txt` lowered (Texturing 1 -> 0 references and 0 members, TOTAL 652 -> 651 and 98 -> 97).

`Texture3dChannels_Session`, `_NoSession` and `_Unknown` (green since Phase 1, byte-identical to the Phase 1 capture), `Visual_sphere-texture3d-bake3d-reyes{,-slo}`, `test_data_pointcloud` and the two `DeterminismSweep_sphere-texture3d-bake3d-reyes{,-slo}` tests are green after the change.

**Mutations (T100)**, reverted afterwards (the source equals its backup): the core source answering `TYPE_FLOAT` for every channel: `Texture3dChannels_Session` and `Texture3dChannels_Unknown` red (the expected text differs). The core source returning a **copy** of `fill` instead of the registry's array: **all three tests stay green**, so the aliasing is not covered by an existing test (`Texture3dChannels_Session` does not read the fill values: its header says so). Reported for the maintainer: a test that compares the pointer `find()` returns with the registry's `fill` would cover it.

Architecture (CP-c): the three DB-021 lines are gone from `ALLOWED` (`DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-010 x 8 DB-011 x 173 DB-015 x 10 DB-017 x 78 DB-025 x 12`), `METRIC Texturing refs 0 (baseline 0)` after lowering, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`, no `unclassified:`. Report: `build-037-snapshots/archcheck-p6.txt`.

Full macOS run after Phase 6 and the T093 corrections (`ctest -LE determinism-sweep -j4`): 953 registered, 951 passed; every test of the Phase 5 pass list still passes; the two failures are `PluginAbi_DocPaths` and `PluginAbi_InstallHeaders` (Phase 7). `DisplayV1_SerializedData`, `DisplayAbi_Hostile` and `DisplayV1_HostileCopiesOnly` are green with the approved corrections (T093a closed). Linux was first unreachable (`ubuntu-2404-desktop.v2-labs.io` returned `NXDOMAIN` for a while); it ran after the host came back, see "Phase 6 Linux" below. The helper `orender-fb`: nothing in this branch stops it at the end of a render (`fbipc_display.cpp` is unchanged; its only `kill` is the pre-existing SIGTERM of a freshly spawned helper that never accepted the connection within 5 s).

### Phase 6 Linux (T093a, T100, T100a)

`openrender-037`, re-synced from the staged tree (`rsync -rlpc --files-from`, 3227 files, checksums verified, no file deleted since the last sync); build under GCC 13 `-Werror` at `-j2` through the remote `cpujob`: clean, 0 warnings. The T093a tests `DisplayV1_SerializedData`, `DisplayAbi_Hostile` and `DisplayV1_HostileCopiesOnly` pass. Full run (`ctest -LE determinism-sweep -j2`) with the Phase 6 tree: 954 registered, **945 passed**, 9 failed, which are exactly the seven known Linux baseline failures (`LibShader_McompConsistency`, `TextureTile_RenderByteIdentical`, `TextureTile_CacheByteIdentical`, `_CacheDefaultDirectory`, `_CacheStaleDetection`, `_CacheConcurrentWrite`, `CoverageAnalytic_alpha-sides2-tjunction-r4_reyes`) and the two Phase 7 install tests (`PluginAbi_DocPaths`, `PluginAbi_InstallHeaders`); compared with the Phase 5 Linux run, the three display defects are gone and nothing new fails (the `Visual_sphere-texture3d-bake3d-reyes` race did not show this time). `ctest -L architecture`: 21/21.

**T100a, `Texture3dChannels_FillAliased`** (added at the checkpoint 6 approval). A new mode `fillalias` of `test_channels.cpp`; the program decides (exit 0 or 1, no reference file). In a session a channel `_filled` declared with `"float fill" 0.75` and a channel `_plain` with no fill: the pointer `CTexture3dChannelSource::find()` returns as `fill` equals the registry's own `fill` (not a copy), `_plain` has NULL on both sides, name, sample count and type are right, an unknown name answers false, the source is absent before `RiBegin` and again after `RiEnd`. Green on macOS and on Linux (with the other three `Texture3dChannels_*` tests and `Visual_sphere-texture3d-bake3d-reyes{,-slo}`). **Mutation** (the core source returns a copy of `fill`, reverted; the source equals its backup): only the new test turns red, with `find() hands out the registry's own fill array for _filled, not a copy`; the three older tests and the visual tests stay green, so the new test covers exactly the gap T100 found. `ctest -L architecture`: 21/21 on macOS and on Linux.

### Phase 7 red evidence (T102 to T104)

Six tests, run on macOS (`cpujob 037-p7-red ctest --test-dir build -R 'PluginAbi_(InstallHeaders|InstallList|NoFileBase|Shadeop|InstallLayouts|DocPaths)' -j2`) and on Linux (`openrender-037`, GCC 13, remote `cpujob`, same command). Both platforms give the same table. No implementation task has run: `examples/display/simpledisplay.c` does not exist, the install rules are untouched.

| Test | Result | Why it is red (verbatim from the log) |
|---|---|---|
| `PluginAbi_InstallHeaders` (changed) | red | `selfcontained: .../examples/display/simpledisplay.c does not exist: the guide's example display (T103)` and the same for `fhs` |
| `PluginAbi_InstallList` (new) | red | `src/display/file/CMakeLists.txt: install(FILES file_base.h) puts a header into the include directory that is not in the installed header list`; `src/libshader/runtime/CMakeLists.txt: install(FILES rslo.h) ...` (same text); `INSTALL_ARTIFACTS.md (section) names file_base.h, which is not in the installed header list`; `INSTALL_ARTIFACTS.md (section|tree) does not name orplugin.h / orplugin.hpp`; `DISPLAY_PLUGIN_GUIDE.md names file_base.h`; `#include of ri/dsply.h | file_base.h | file/file_base.h names no header of the installed list` |
| `PluginAbi_NoFileBaseInstalled` (new) | red | `src/display/file/CMakeLists.txt: install() names file_base.h` |
| `PluginAbi_ShadeopSoversion` (new) | **green** | `libRIBShadeop` already carries `OPENRENDER_COMPAT_SOVERSION` (SC-019 holds on the starting tree; the test pins it) |
| `PluginAbi_InstallLayouts` (new, T102a) | red | `selfcontained: the Development component installed nothing: the public headers and the package files must carry COMPONENT Development` and the same for `fhs` (the install rules have no component tag; `cmake --install --component Development` installs nothing, so the FHS include directory, the nothing-else-in-`<prefix>/include` rule and the `find_package(openRender)` consumer are not reached) |
| `PluginAbi_DocPaths` (unchanged, known red) | red | the five stale references: guide `#include "ri/dsply.h"`, `#include "file_base.h"`, `#include "file/file_base.h"`, `` `src/framebuffer/` ``; `INSTALL_ARTIFACTS.md` `` `src/file/CMakeLists.txt` `` |

Seen while writing the tests (not part of the red table):

- The old `PluginAbi_InstallHeaders` compiled `dsply.h` as the main file, which trips `-Wunused-function` for the `static inline orDisplayFindParameter` (a check artifact, not a header defect); the new script compiles a translation unit that includes the header.
- The script that builds and runs the example (compile as a module with only `-I <layout include dir>`, render `Display "example.ppm" "simpledisplay" "rgb"` 8x4 through `orender`, read a P3/P6 header `8 4`) was exercised with a **scratch** example kept outside the repository: both layouts pass, so the red above is the missing example and nothing else. The example in the repository is T103's, written after the approval.
- `PluginAbi_InstallLayouts` runs a nested configure of the source tree per layout (`-DINSTALL_SELFCONTAINED=ON|OFF`, `-DCMAKE_INSTALL_PREFIX=<scratch>`, build type, compilers, toolchain and the `OPENRENDER_*` options copied from the main cache) under `build/tests/plugins/checks/install_layouts/`, then `cmake --install <tree> --component Development`; it never builds a library. About 15 s on macOS, about 80 s on Linux. Every path it writes is under the build tree (`/usr/local/include` and the main `CMAKE_INSTALL_PREFIX` were checked untouched).
- `rslo.h` is installed into the include directory by `src/libshader/runtime/CMakeLists.txt:62`, is not in `OPENRENDER_PUBLIC_HEADERS` and is not documented: the tests (installed set == list) are red on it. Decision for the maintainer (see the report): add `rslo.h` to the list and the documents, or stop installing it.

### Phase 7 green evidence (T103 to T107a)

The maintainer approved the Phase 7 tests (T104) and the four recommendations: `rslo.h` is installed and joins `OPENRENDER_PUBLIC_HEADERS`; the four stale references `PluginAbi_DocPaths` reported are fixed now (new task T106a), so `DocPaths` is green in this phase; the nested-configure approach of `PluginAbi_InstallLayouts` stays; the header installs, `install(EXPORT ...)` and the package config files carry `COMPONENT Development`.

Implemented: `examples/display/simpledisplay.c` and `examples/display/README.md` (T103); the `install(FILES file_base.h ...)` rule removed from `src/display/file/CMakeLists.txt` and the `rslo.h` rule removed from `src/libshader/runtime/CMakeLists.txt` (it installs with the list), `OPENRENDER_PUBLIC_HEADERS` final with `rslo.h` as the ninth entry (T105); one variable, `OPENRENDER_INCLUDE_INSTALL_DIR` (`CMake/openRenderPublicHeaders.cmake`: `${CMAKE_INSTALL_INCLUDEDIR}` for the self-contained layout, `${CMAKE_INSTALL_INCLUDEDIR}/openRender` for FHS), used by the header `install(FILES ...)` and by the `INCLUDES DESTINATION` of every exported target (`ribRender`, `ribVector`, `ribOut`, `libshader_*`, and now also `openrendercommon` and `openrendershadeop`, shared and static, which had none), and by `CMake/openRenderConfig.cmake.in` through `PATH_VARS`, which defines `openRender_INCLUDE_DIR` (T105a); `INSTALL_ARTIFACTS.md` (T106) and the four path fixes (T106a: guide `#include "dsply.h"`; the `CFileOutputBase` section says the helper is in-tree and not installed and no longer names the file; `src/display/framebuffer/`).

Two corrections to the approved tests, found when they first ran against an implementation (neither loosens a check): `install_consumer/CMakeLists.txt` linked the imported targets, which has no library file to link once the per-config targets files are removed, so it now takes the exported `INTERFACE_INCLUDE_DIRECTORIES` of the eight shared targets and `openRender_INCLUDE_DIR`, **checks that each equals the layout's include directory**, and compiles the consumer with that directory only (a stronger check than before); `stage_headers.cmake` gained `src/libshader/runtime` in its lookup directories, which `rslo.h` needs. The three scripts that use `IN_LIST` set policy CMP0057.

**Ratified by the maintainer** (relayed by the coordinator): the three edits to approved Phase 7 test code, `install_consumer/CMakeLists.txt`, `check_install_layouts.cmake` and `stage_headers.cmake`. They were applied before the diff was shown; from now on an approved test, script or reference is edited only after its diff has been shown and approved.

macOS: build clean (0 warnings, `-Werror`). `PluginAbi_DocPaths`, `_InstallHeaders`, `_InstallList`, `_NoFileBaseInstalled`, `_ShadeopSoversion`, `_InstallLayouts` all pass (InstallLayouts about 15 s). Full run `ctest -LE determinism-sweep -j4`: 958 registered, **957 passed, 0 failed, 1 skipped** (`LibShader_Format_rslo_lower_minor`, which skips by itself and is not in the Phase 6 pass list either). Against `pass-list-p6.txt`: nothing lost; gained the six tests above and `Texture3dChannels_FillAliased`. `ctest -L plugin_abi`: 63/63. `ctest -L architecture`: 21/21; the full report (`build-037-snapshots/archcheck-p7.txt`): `ALLOWED DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-010 x 8 DB-011 x 173 DB-015 x 10 DB-017 x 78 DB-025 x 12`, `METRIC TOTAL refs 651 (baseline 651) distinct 97 (baseline 97)`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`.

**Mutation (T107)**, reverted afterwards (the file equals its backup): `file_base.h` added to `OPENRENDER_PUBLIC_HEADERS` in a scratch edit. `PluginAbi_InstallHeaders` red (`file_base.h: #include "common/os.h" names a header that is not in the installed set`, and it does not compile alone as C++20 or C17, in both layouts); `PluginAbi_InstallList` red (`INSTALL_ARTIFACTS.md (section|tree) does not name file_base.h, which is in the installed header list`); `PluginAbi_NoFileBaseInstalled` red (`file_base.h is in the public header list`).

**Install locations (T107a, SC-011)**: both scratch installs ran green; the files below are what `cmake --install --component Development` put under each scratch prefix (`build/tests/plugins/checks/install_layouts/{selfcontained,fhs}/prefix`); nothing was written outside the build tree (the install manifests hold only paths under the scratch prefix; `/usr/local/include` is unchanged).

### Phase 7 Linux (T108)

`openrender-037` was synced from the staged tree (`rsync -rlpc --files-from`, then `sha256sum -c`: every file OK; no file deleted since the last sync). It was unreachable for a while (the host name did not resolve), so the gate ran after the first Phase 7 report. GCC 13 `-Werror`, `-j2` through the remote `cpujob`: configure and build clean, 0 warnings. `ctest -LE determinism-sweep -j2`: 959 registered, **951 passed, 7 failed, 1 skipped**; the seven failures are exactly the Linux baseline (`LibShader_McompConsistency`, `TextureTile_RenderByteIdentical`, `TextureTile_CacheByteIdentical`, `_CacheDefaultDirectory`, `_CacheStaleDetection`, `_CacheConcurrentWrite`, `CoverageAnalytic_alpha-sides2-tjunction-r4_reyes`); the skip is `LibShader_Format_rslo_lower_minor` (as on macOS). The two Phase 6 reds, `PluginAbi_DocPaths` and `PluginAbi_InstallHeaders`, are green. The six Phase 7 tests pass (`InstallLayouts` takes about 96 s on Linux). `ctest -L architecture`: 21/21; the full report: `ALLOWED` unchanged, `METRIC TOTAL refs 651 (baseline 651) distinct 97 (baseline 97)`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`. Install check: both install manifests list only paths under their scratch prefix (0 outside), the prefixes hold exactly the nine headers and the four package files, `/usr/local` on the host has no file newer than the sync and its `include` is empty.

### Phase 8 red evidence (T108a, T108b)

Two tests, both run on macOS and on Linux (`openrender-037`, GCC 13), identical results. Command: `ctest -R 'PluginAbi_(DocPaths|RegistryConsistency|VersionHistory)' --output-on-failure`. No implementation task of Phase 8 has run.

| Test | Result | Why it is red (from the log) |
|---|---|---|
| `PluginAbi_DocPaths` (extended) | red | `DEVNOTES_DETAILS/PLUGIN_ABI.md: the document does not exist`; `docs/site/content/manual/reference/plugin-abi.md: the document does not exist`; `DISPLAY_PLUGIN_GUIDE.md: states no display version (expected 'display 2.0.0')`; the two missing documents state none of the versions they must. The 45 path references of the existing four documents are fine; 0 version statements exist yet |
| `PluginAbi_RegistryConsistency` (new) | red | DB-003, DB-014 and DB-021 are not under `## Resolved`; DB-003's rule text lacks "a MAJOR the host supports"; the preamble names v1.2.0+, not v1.3.0+; DB-019's Size still says "3 include edges"; DB-024 has no History entry; `DEVNOTES.md`: no status-table row mentions spec 037 |
| `PluginAbi_VersionHistory` (unchanged) | green | the headers' macros and history tables are consistent (the source of the numbers the documents quote) |

The tree half of `RegistryConsistency` (`texture3d.cpp` includes neither `renderer.h` nor `displayChannel.h`; no file of `src/libshader` includes the old `src/ri/parse/dso.h`) and its allowlist half (no line for DB-003, DB-014, DB-019, DB-021, and every line's id an Open entry) are already green: the allowlist ids are `DB-001 DB-004 DB-005 DB-006 DB-010 DB-011 DB-015 DB-017 DB-025`.

Design of the version comparison (for approval). A document *states a version for an interface* in two forms: prose, the interface word (`display`, `shadeop`, `dlo`, `dynamicload`, `implicit`), optional qualifier words (interface, ABI, API, version, table, module, driver(s), operand(s), plug-in(s), procedural, surface, DSO), then `M.m.p`, markup and case ignored ("display 2.0.0", "the shadeop interface version 1.1.0"), which must equal the header's current `OR_ABI_VERSION` macro; and a tagged history row `| M.m.p (key) |`, which must be a row of that interface's history table in the header (any row, so a historical version quoted from the table is accepted). A historical version in prose is written "MAJOR 1", not "display 1.0.0". `REQUIRE` makes a missing statement a failure: the display guide must state `display`; `PLUGIN_ABI.md` all five interfaces; the site page `display` and `shadeop`. A document that does not exist is a failure (it used to be a skip, because Phase 7 had not written them). The logic was exercised against a scratch document outside the repository: 10 statements compared, three wrong ones reported (`display 1.0.0`, `shadeop 1.3.0`, the row `2.5.0 (display)`), the correct ones and the unrelated release number `1.2.0` accepted.

### Phase 8 green evidence (T108b to T116)

The maintainer approved the Phase 8 tests (PAUSE 8a): the extended `PluginAbi_DocPaths` and the new `PluginAbi_RegistryConsistency` as designed (T108b closed). The condition on DB-019 was checked before the registry edit (T114): the compiler includes no Scene Interface or renderer-core header any more (`grep` over `src/libshader/compiler`: nothing outside Shading, Infra and `shadeop/shadeopModule.h`), it no longer `dlopen`s shadeop modules itself, and the allowlist has no DB-019 line, but two real items remain, so DB-019 **stays Open** with them written into the entry: the JIT has no DSO shadeop call path (the two `XXX` dispatcher rows of `scriptFunctions.h` have no JIT emission) and the compiler still takes its builtin and opcode tables from the shading engine's headers instead of owning them (specs 038 and 043). The test's DB-019 expectation (Open, Size of 0 edges) therefore needed no change.

Implemented: `DEVNOTES_DETAILS/DISPLAY_PLUGIN_GUIDE.md` rewritten for display 2.0.0 (T109); `DEVNOTES_DETAILS/PLUGIN_ABI.md` (T110); `docs/site/content/manual/reference/plugin-abi.md`, linked from `display-drivers.md` and `dso-shading.md` (T111); `NEWS.md` (new `## Unreleased` section) and `ChangeLog.md` (the existing `[Unreleased]` section) (T112); a status-table row in `DEVNOTES.md` (T113); DB-003, DB-014 and DB-021 moved to Resolved in `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` with the date 2026-10-31, the rule text and the preamble updated (v1.3.0+), DB-019 Evidence and Size updated, DB-024's History records the display ABI item as done (T114); `tests/architecture/allowlist.txt` unchanged in this phase (`git diff 486decd..HEAD`: 6 lines deleted, 0 added) and `metrics-baseline.txt` needs no change (archcheck reports `METRIC TOTAL refs 651 (baseline 651) distinct 97 (baseline 97)`, no `NOTE`); the CLAUDE.md draft is `specs/037-plugin-abi-semver/claude-md-draft.md` (T116; `CLAUDE.md` itself is untouched).

`ctest -R 'PluginAbi_(DocPaths|RegistryConsistency|VersionHistory)'`: all three green (`DocPaths` compares 28 version statements and checks 95 path references in the four documents). **Mutation (T115)**, reverted afterwards (the guide equals its backup): "The interface is display 2.0.0." changed to "display 2.1.0" in the guide: `PluginAbi_DocPaths` red with `DISPLAY_PLUGIN_GUIDE.md: 'display 2.1.0' but the header's display version is 2.0.0`.

`NEWS.md` and `ChangeLog.md` entries stand under "Unreleased" headings, never under a release number. One latent defect of the approved `PluginAbi_RegistryConsistency` was found while writing the `DEVNOTES.md` row and left unfixed pending approval: the status-table rows are collected in a CMake list, so a row that contains a semicolon is split in two and the test reads half a row (the row was written without semicolons). Proposed one-line fix, not applied: replace `;` by `@@SEMI@@` right after `file(READ ... _devnotes)` and restore it where `_row` is read.

A second latent defect, in the approved `PluginAbi_InstallList` and `PluginAbi_NoFileBaseInstalled`, showed once (in the `build-tsan` tree, `-j2`, `InstallLayouts` running at the same time): both collect the CMake files with `file(GLOB_RECURSE "${SOURCE_DIR}/CMakeLists.txt" ...)`, which recurses through the whole source tree, build directories included, so it can open a `CMakeScratch/TryCompile-*/CMakeLists.txt` of the nested configure of `PluginAbi_InstallLayouts` that vanishes while it reads (`file failed to open for reading`). The test passed on re-run. Proposed fix, not applied (approved tests; diff for the maintainer): in `check_install_list.cmake` line 22 and `check_no_file_base_installed.cmake` line 15 replace the root pattern by a non-recursive glob, `file(GLOB _cmake_files "${SOURCE_DIR}/CMakeLists.txt")` then `file(GLOB_RECURSE _cmake_tree "${SOURCE_DIR}/src/*CMakeLists.txt" "${SOURCE_DIR}/CMake/*.cmake" ...)` and `list(APPEND _cmake_files ${_cmake_tree})`.

Site build (T111): `hugo` is installed (`/opt/homebrew/bin/hugo`). `hugo --baseURL "/" --destination <scratch>` builds the site (the new page renders to `manual/reference/plugin-abi/index.html`). `docs/tools/link-validator.sh` on that output exits 1 with 163 broken links, all of the classes that exist on the unmodified site (the hard-coded `/openrender/static/images/...` image links of `global-illumination`, `/favicon.png` on every page, and the home page's own absolute links); the three pages this phase touches report only `/favicon.png` (the same entry every page has), so the phase adds no broken link of its own. The CI deploy build (`--minify --baseURL <pages url>/`) was not run.

#### Checkpoint 8 approvals (2026-11-01)

The maintainer approved the checkpoint 8 recommendations: (1) the `hiderFlags` race is fixed in 037 with the smallest atomic change, no nesting of `jobMutex` inside `displayKillMutex`, **the diff to be shown and approved before it is applied** (task T128); (2) the other thread-sanitizer reports are grouped into distinct report sites for the maintainer to file as issues (no GitHub write is made by the agent); (3) the flaky-test fix is applied to `check_install_list.cmake` and `check_no_file_base_installed.cmake` (no `GLOB_RECURSE` on the top-level `CMakeLists.txt`); (4) the semicolon fix is applied to `check_registry_consistency.cmake`; (5) `claude-md-draft.md` was not approved at that point; later the same day the coordinator applied it to the worktree's `CLAUDE.md` on the maintainer's instruction (T116 applied; the stale `framebuffer/` and `file/` lines under `src/` in the layout tree were replaced by the `src/display/` line). The implement agent has not edited `CLAUDE.md`. Items 3 and 4 are applied exactly as the diffs shown in the checkpoint 8 report.

#### Thread sanitizer report sites (checkpoint 8 follow-up)

The sanitizer run was repeated on a rebuilt `build-tsan` (`TSAN_OPTIONS=abort_on_error=0:halt_on_error=0:exitcode=0:log_path=...`, `ctest -L plugin_abi -j2`: 64/64 pass): 176 reports (the count varies from run to run with thread timing; the first run had 163), which fall into **four distinct sites**. None has a display-host, v1-adapter, shadeop-module, procedural-module or channel-source frame. The maintainer will file them; the agent makes no GitHub write.

| # | Reports | Access A (file:line) | Access B (file:line) | Memory | Assessment |
|---|---|---|---|---|---|
| 1 | 101 | `CPatch::CPatch` `surface.cpp:69` (read, `stats.numSurfaces > stats.numPeakSurfaces`) and `surface.cpp:70` (write, `stats.numPeakSurfaces = stats.numSurfaces`): 61 reports across the 69/70 pairs | `CPatch::splitToChildren` `surface.cpp:777`, `:800`, `:836` (writes, `stats.numSplits++`): 40 reports across the pairs of these three lines | the global `stats` (`libRIBCommon`) | benign: unsynchronized statistics counters incremented by the REYES threads (lost updates change only the printed statistics); not plug-in code |
| 2 | 65 | `CStochastic::drawQuadGridZmin` `stochasticQuad.h:798` (read, `drawPixelCheck()`; reached from `rasterDrawPrimitives` `stochasticPrimitives.h:26`) | `movvv` `mathSpec.h:148`/`:149` (write), from `CReyes::copySamples` `reyes.cpp:1187` in `CReyes::shadeGrid` `reyes.cpp:1068` (reached from `drawQuadGridZminUnshaded`) | the vertex data of a REYES grid (heap block from `CReyes::newGrid` `reyes.cpp:1291`) | real: one thread lazily shades a shared grid (under the grid's mutex) while another thread rasterizes it without synchronizing with that shading |
| 3 | 9 | `CReyes::shadeGrid` `reyes.cpp:1130` (write, `grid->flags &= ~(RASTER_UNSHADED | ...)`, under the grid mutex) | `CStochastic::rasterDrawPrimitives` `stochasticPrimitives.h:24` (read of `grid->flags`, unlocked) | the grid object (heap block from `CReyes::newGrid` `reyes.cpp:1281`) | real: the unlocked test of `RASTER_UNSHADED` is a double-checked pattern with no acquire or release; with site 2 it means a reader can see the flag cleared (or the data) without a happens-before edge. Probably harmless on x86, not guaranteed on arm64 |
| 4 | 1 | `CRenderer::dispatch` `rendererDisplay.cpp:354` (write, `hiderFlags |= HIDER_BREAK` under `displayKillMutex`) | `CRenderer::dispatchReyes` `rendererJobs.cpp:81` (read, `hiderFlags & (HIDER_DONE | HIDER_BREAK)` under `jobMutex`) | the global `CRenderer::hiderFlags` | real, small: a flag read-modify-write under one mutex against a read (and the `rendererJobs.cpp:135` read-modify-write) under another; a lost `HIDER_BREAK` or `HIDER_DONE` is possible. This is the only site in the display path; fix proposed as T128 |

#### macOS full pass (T117 to T121)

- **Build (CP-a)**: clean, 0 warnings, `-Werror`.
- **Suite (CP-b)**: `ctest -LE determinism-sweep -j4`: 959 registered, **958 passed, 0 failed, 1 skipped** (`LibShader_Format_rslo_lower_minor`, which skips by itself). Against `pass-list-p7.txt` (the Phase 7 pass list): nothing lost, `PluginAbi_RegistryConsistency` gained. `ctest -L plugin_abi`: 64/64. The known-red table is empty. No threshold was widened and no test disabled.
- **`DISABLED` / `EXCLUDE_FROM_ALL` (T117)**: `grep -rnE 'DISABLED|EXCLUDE_FROM_ALL' tests/plugins` finds only the `EXCLUDE_FROM_ALL` option of the `or_plugin_test_module` helper (`tests/plugins/CMakeLists.txt`; no call passes it any more) and comments recording that T047 and T061 removed the markers (`tests/plugins/versioncheck`, `tests/plugins/procedures`); no `DISABLED` property remains, and no ctest summary reports a "Not Run (Disabled)" test.
- **Architecture (T118)**: `ctest -L architecture`: 21/21. Full report (`build-037-snapshots/archcheck-p8.txt`): `ALLOWED DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-010 x 8 DB-011 x 173 DB-015 x 10 DB-017 x 78 DB-025 x 12`, `METRIC TOTAL refs 651 (baseline 651) distinct 97 (baseline 97)`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; no `STALE`, no `unclassified:`; the six deleted allowlist lines are gone (3 DB-019, 3 DB-021), none added, and `src/shadeop/` and `src/procedures/` are mapped (`ShaderDSO`, `Procedures`).
- **Determinism (T119)**: `-L '^determinism$' -j2`: 155/155 (139 s); `-L '^determinism-sweep$' -j2`: 419/419 (564 s).
- **Real install, default layout (T121)**: `DESTDIR=build/install-stage cmake --install build` (self-contained, prefix `/usr/local`): exit 0; `find build/install-stage -name file_base.h` finds nothing; the include directory holds `dlo.h dsply.h implicit.h orplugin.h orplugin.hpp ptcapi.h ri.h rslo.h shadeop.h`; `libRIBShadeop` is installed (`libRIBShadeop.1.0.0.dylib`, `.1.dylib`, `.dylib` and `.a`, SOVERSION 1 like `libRIBCommon`); the package files are present. `examples/display/simpledisplay.c` builds against the staged include directory alone (`-std=c17 -Wall -Wextra -Wpedantic -Werror -bundle`), and `orender` from the staged tree (through `cpujob 037-install-render`) renders `examples/rib/camera-dof.rib` with `Display "stage-dof.ppm" "simpledisplay" "rgb"` into a 400 x 300 P6 file. Nothing landed outside `build/install-stage`: the mtimes of `/usr/local` (Sep 29 11:37:44 2026), `/usr/local/include` and `/usr/local/lib` (Oct 2 10:44:42 2023) and `/usr/local/bin` (Oct 16 19:16:45 2026) are the same before and after, and `/usr/local/include` is still empty.

#### Thread sanitizer (T120, SC-006)

`cmake -B build-tsan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENRENDER_TSAN=ON -DOPENRENDER_ENABLE_JIT=OFF`; the first build failed in `orender-wire-macos` (swift links `libribpreview.a` without the sanitizer runtime: unresolved `___tsan_*` symbols), so `src/preview/CMakeLists.txt` now leaves the macOS viewer out of a `OPENRENDER_TSAN` build (it is no plug-in path); the build then succeeded at `-j2`. `ctest -L plugin_abi -j2` with the default sanitizer settings: 61/64; `DisplayV1_SerializedData`, `ShadeopAbi_LegacyShared` and `ShadeopAbi_Versions_Render` fail because the first race report aborts `orender` on macOS (`abort_on_error` is the default). To see every report the run was repeated with `TSAN_OPTIONS=abort_on_error=0:halt_on_error=0:exitcode=0:log_path=...`: 64/64 pass and 163 reports in 10 log files. **None has a display-host, v1-adapter, shadeop-module, procedural-module or channel-source frame in either access stack.** They are: 85 on the global `stats` (`libRIBCommon`, unsynchronized counters incremented by the REYES threads: `CPatch::CPatch`, `splitToChildren`), and 77 on REYES heap blocks (`stochasticQuad.h:798` `drawQuadGridZmin`, `reyes.cpp:1130` `shadeGrid`, `stochasticPrimitives.h:24`) and one on the global `CRenderer::hiderFlags`. No suppression was added. **Finding for the maintainer**: the `hiderFlags` report is in the display path, `CRenderer::dispatch` (`rendererDisplay.cpp:354`): when the last display disables itself the core sets `HIDER_BREAK` under `displayKillMutex`, while `dispatchReyes` (`rendererJobs.cpp:81`) reads `hiderFlags` under `jobMutex`. The same write under the same mutex is in the base tree (`git show 486decd:src/ri/render/rendererDisplay.cpp`, lines 351-355), so spec 037 did not introduce it, but SC-006 asks for no race in the display paths, so it needs a decision (take `jobMutex` around the write, or make `hiderFlags` atomic); the other 162 reports are renderer internals unrelated to the plug-in boundary and predate the spec.

## 3. Phase 1 reference inventory (T029)

Captured by `cpujob 037-p1-capture5 cmake --build build --target plugin-abi-capture-baselines -j1`, then a second capture (`plugin-abi-capture-baselines-scratch`, into the build tree) compared with `diff -r` / `cmp`: **identical sample for sample** for all pixel references, `missing_symbol.txt` and the three Texture3d reports. The target **refuses to overwrite**: running it again on the existing references fails with `<file> exists; refusing to overwrite (configure with -DOR037_RECAPTURE=ON ...)` for every scene (observed).

`tests/plugins/display_baseline/reference/macos/` (decoded pixels, header `ORPIX1 <w> <h> <channels> <bits> <float>` plus the native samples), 184 KiB on disk, 18 files:

| Scene | Driver | Layout |
|---|---|---|
| `tiff8`, `dither`, `typetiff`, `notype`, `png8`, `gamma` | file | 32 x 32 x 4, 8-bit |
| `tiff16`, `quantline`, `png16` | file | 32 x 32 x 4, 16-bit |
| `tifffloat` | file | 32 x 32 x 4, float |
| `depth` | file | 32 x 32 x 1, float |
| `aov` | file | 32 x 32 x 7, 8-bit (`rgba,N`) |
| `exrhalf` | openexr | 32 x 32 x 4, float (the driver stores half; the decoder delivers float) |
| `rgbe` | rgbe | 32 x 32 x 3, float |
| `dlo_located`, `implicit_located`, `dynamicload_plain` | file | 32 x 32 x 4, 8-bit (procedure scenes) |
| `gamma.gamma` | | PNG gAMA chunk value **220000** (gamma 2.2 x 100000) |

Other captured data: `tests/plugins/scenes/reference/missing_symbol.txt`, `tests/plugins/texture3d/reference/{session,nosession,unknown}.txt`.

The frozen legacy driver (`display/legacy_v1/`, copies of the base tree's `file.cpp`, `file.h`, `file_tiff.*`, `file_png.*`, `file_base.*` and `dsply.h` as `dsply_v1_frozen.h`; `file.h` is copied although T008 does not list it) renders the 11 file-driver scenes: all **identical** to the references the live driver produced (`DisplayAbi_LegacyPixelsIdentical` green, gAMA 220000 equal).

Things the Phase 1 work found on the starting tree (not fixed; for the maintainer):

1. `Geometry "dlo"` under `Hider "raytrace"` ends in a **bus error** after the intersections succeed (CDLObject's shading path); no DLO raytrace scene exists, only REYES (which draws nothing: `CDLObject::dice` is empty).
2. The old `dsoParse` returns a **stale prototype tail**: its static buffer is never NUL-terminated, so a declaration shorter than the previous one comes back with the previous one's tail. Measured: `float longer(float, float, float)` then `void shorter()` gives prototype `o=fffpcmso` instead of `o=`. The characterization table is ordered by increasing prototype length so it does not depend on this; the test prints the observation.
3. `ptcapi.h` declares `PtcCreatePointCloudFile(..., const char **, const char **, ...)` and `PtcOpenPointCloudFile(char *, int *, char **, char **)`; the library defines `char **` (create) and `const char **` (open) overloads. The C symbols the header names are not defined (`nm`: only `__Z23PtcCreatePointCloudFilePciPS_S0_PfS1_S1_` and `__Z21PtcOpenPointCloudFilePcPiPPKcS3_`), so the installed header cannot be linked against for those two. The Texture3d program declares the overloads that exist.
4. The point cloud counts and reads back fewer points than were written (3 written, `npoints` 2, one point read back by `PtcReadDataPoint`); characterized as it is.
5. Texture3d fields not observable without new API: per-channel sample counts, start offsets and the `fill` values (`CTexture3d::CChannel` is a protected nested class; `queryChannels` returns names and type strings only). The reports cover names, type strings, the data size, the point count and the data of the point read back.
6. The openexr driver always writes half-float; there is no "OpenEXR float" scene (research R16 and T023 say half/float). `quad` is not a token of the declaration grammar, so FR-040's test has `double` and `mpoint` cases only.
7. A RIB without a `Display` line falls back to a default display that is not found in the build tree (`Failed to open out device "file"`, exit 255); every fixture RIB names a display search path and a `Display`.

## 4. Mutation checks and greps

| Check | Mutation | Result |
|---|---|---|
| `Texture3dChannels_*` runner | the expected report edited (`data size 4` -> `5`) | fails: `the report changed` |
| `DisplayAbi_PixelsIdentical` runner | the last byte of `tiff8.pix` flipped in a scratch copy | fails: `1 of 4096 samples differ (largest absolute difference 1)` |
| `ShadeopAbi_DeclarationCharacterization` | (found while writing) a wrong expansion of `{rep:...}` produced a wrong declaration | failed with the expected-vs-got lines, then fixed |
| capture target | run on existing references | refuses, exit 2 |
| Phase 1 diff outside `tests/` and `specs/` | `git status` shows `tests/CMakeLists.txt`, `tests/plugins/`, `specs/037-plugin-abi-semver/` only (and the untracked `build*` directories, ignored) | no file under `src/`, `examples/`, `CMake/` or the root `CMakeLists.txt` changed |

### T084: who can call dispatch, and when (Phase 5)

Read before the host moved (research M10). `CRenderer::renderFrame` (`renderer.cpp:1049`) starts the render threads (`rendererDispatchThread`, or `serverDispatchThread` for the network-server path) and joins every one with `osWaitThread` (lines 1070 and 1100) before it returns. `endFrame()` runs after `renderFrame()` and ends with `endDisplays()` (line 872), after `root->destroy()` and `releaseFrameProceduralModules()`: no render thread exists when the displays are finished and their modules released. The callers of `dispatch`: `commit()` and, through it, `clear()` (the border clears), and `commit()` is called from `CReyes::` (`hiders/reyes.cpp:459`) and `CRaytracer::` (`hiders/raytracer.cpp:472`) on the render threads, and from `serverDispatchThread` (`rendererJobs.cpp:345`) for a server's buckets, all inside `renderFrame()`. The displays are started in `computeDisplayData()` (called from `beginFrame()` through `beginDisplays()`), before any of those threads exists. With a network client `commit()` returns before it reaches `dispatch`, and no display is started (`netClient != INVALID_SOCKET` has no plug-in). So `dispatch` is only called while the host is started, and `finishAll()` has no call in flight. No path called for a stop.

## 5. Architecture reports and metrics

| Checkpoint | `ctest -L architecture` | Report |
|---|---|---|
| Starting tree (T004) | 21/21 | `tools/archcheck-base-macos.txt` |
| Phase 1 (T031 CP-c) | 21/21 | identical to the starting-tree report (`diff` empty); `tests/plugins` produced no `unclassified:` line; no `STALE`, no `VIOLATION` |
| Phase 2 (T048 CP-c) | 21/21 | `ALLOWED` line identical (no allowlist line added or removed), `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; `METRIC` lines identical; the object count goes 279 -> 280 (`pluginAbi.cpp`) and the include edges 1534 -> 1538 (the four family headers include `orplugin.h`, all `* -> Infra`), and one informational line is added: `UNRESOLVED src/common/orplugin.hpp (no compiled translation unit includes it)`, like the ten such header lines already in the base report. No `unclassified:` (`prefix src/common/ Infra` covers the new files) |

## 6. macOS final pass

### Install include directories (T107a, SC-011)

| Layout | Configure | Include directory (under the prefix) | Holds |
|---|---|---|---|
| self-contained | `-DINSTALL_SELFCONTAINED=ON` | `include/` (`<ORENDERHOME>/include`) | `dlo.h dsply.h implicit.h orplugin.h orplugin.hpp ptcapi.h ri.h rslo.h shadeop.h` |
| FHS | `-DINSTALL_SELFCONTAINED=OFF` | `include/openRender/` | the same nine files; nothing else directly in `<prefix>/include` |

The package files go to `lib/cmake/openRender/` in both layouts. A `find_package(openRender CONFIG)` consumer sees `openRender_INCLUDE_DIR` and the `INTERFACE_INCLUDE_DIRECTORIES` of all eight shared targets equal to that directory and compiles `#include "dsply.h"` and `#include "orplugin.h"` with it and no other flag.

Final pass (T117 to T121, T126): see "Phase 8 green evidence" in section 2 for the macOS full pass (958 of 959 passed, 1 skipped, `plugin_abi` 64/64, architecture 21/21, determinism 155/155 and 419/419, thread sanitizer, real install) and "Phase 8 Linux gate" in section 7 for Linux (952 of 960 passed, the 7 baseline failures only, 1 skipped; `plugin_abi` 64/64; architecture 21/21; determinism 155/155 and 419/419; FHS install). No threshold was widened, no test disabled, no reference regenerated.

#### Coverage (T126)

Every requirement and success criterion, with the tasks that implement it and the test or mechanical check that proves it (all in the `plugin_abi` label unless noted; a green run of each on both platforms is in the sections above).

| Requirement | Tasks | Proving test or check |
|---|---|---|
| FR-001, FR-002 | T039, T041, T042, T043, T066, T085 | `PluginAbi_VersionHistory`, `PluginAbi_HeadersC17`, `PluginAbi_HeadersCxx20`, `DisplayAbi_Versions_V2`, `ShadeopAbi_Versions_Registry`, `ProceduresAbi_Versions` |
| FR-003, FR-004 | T035, T040, T052, T064, T079 | `PluginAbi_VersionCheck`, `PluginAbi_SingleCheckRoutine`, `DisplayAbi_Versions_Reject`, `ShadeopAbi_Versions_Render`, `ProceduresAbi_Versions`, `ProceduresAbi_SameRoutine` |
| FR-005 | T036, T041-T044, T047 | `PluginAbi_HeadersC17`, `PluginAbi_HeadersCxx20` |
| FR-006, FR-043, FR-046 | T008, T081, T087, T093, T094 | `DisplayV1_LegacyLoads`, `DisplayV1_Explicit1x`, `DisplayV1_HostileCopiesOnly`, `DisplayV1_SerializedData`, `DisplayV1_CustomDisplay`, `DisplayV1_NoCastCheck` |
| FR-007 | T040, T052, T079, T109, T110, T112 | `DisplayAbi_Versions_V1`, `ShadeopAbi_Versions_Render`, `ProceduresAbi_Versions`, `PluginAbi_VersionCheck` (the 1.2.0 text); the documents by `PluginAbi_DocPaths` |
| FR-008 | T063, T074, T085, T088a, T089-T092, T103 | `ShadeopAbi_ExampleBuilds`, `DisplayAbi_Versions_V2`, `PluginAbi_InstallHeaders` (the example display builds and renders) |
| FR-009 | T042, T051, T052 | `ProceduresAbi_Versions` (dynamicload plain, versioned, v2) |
| FR-010, FR-041 | T034, T039, T040, T079 | `PluginAbi_Convention`, `PluginAbi_VersionCheck`, `DisplayAbi_Versions_Reject` |
| FR-011 | T063, T064, T068, T076 | `ShadeopAbi_PerThread`, `ShadeopAbi_ThreadLifetime` |
| FR-012, FR-037 | T019, T067, T068, T070-T073, T075 | `ShadeopAbi_ParserSingleDefiner`, `ShadeopAbi_ParserNewSymbol`, `ShadeopAbi_ParserReentrant`, `ShadeopAbi_DeclarationCharacterization` |
| FR-013, FR-033 | T015, T064, T068, T076 | `ShadeopAbi_MissingExec`, `ShadeopAbi_ScanOrder` |
| FR-014, FR-015, FR-016, FR-049 | T078-T080, T085, T086 | `DisplayAbi_TwoDisplayParams`, `DisplayAbi_Hostile`, `DisplayAbi_Snapshot`, `DisplayAbi_Descriptors`, `DisplayAbi_ZeroAndDepthOnly` |
| FR-017, FR-018, FR-035 | T017, T080, T086, T088, T093 | `DisplayAbi_LateDisable`, `DisplayAbi_ReleaseAfterFinish`, `DisplayAbi_ReleaseAfterFinish_V2` |
| FR-019, FR-020, FR-039 | T021, T081, T085, T087, T088 | `DisplayV1_LateLookup`, `DisplayV1_LegacyLoads`, `DisplayV1_CustomDisplay` |
| FR-021 | T023-T025, T029, T031e, T088a, T094, T125 | `DisplayAbi_PixelsIdentical`, `DisplayAbi_LegacyPixelsIdentical` |
| FR-022, FR-023, FR-024, FR-032, FR-034, FR-044 | T014, T016, T049-T061 | `ProceduresAbi_BareNameLocated`, `_DynamicLoadRelease`, `_DynamicLoadReleaseRaytrace`, `_ConcurrentAcquire`, `_ObjectAndFrames`, `_MissingSymbol`, `_NoLoadOutsideModule`, `_Scenes` |
| FR-025 | T027, T096-T100 | `Texture3dChannels_Session`, `_NoSession`, `_Unknown`, `_FillAliased`; the `Visual_sphere-texture3d-bake3d-reyes` pair |
| FR-026, FR-038, FR-048 | T020, T045, T102-T107 | `PluginAbi_InstallHeaders`, `_InstallList`, `_NoFileBaseInstalled`, `_ShadeopSoversion`, `_InstallLayouts` |
| FR-027 | T005, every test task | the whole `plugin_abi` label (64 tests, built from the tree's own build files) |
| FR-028 | T109-T113, T115 | `PluginAbi_DocPaths` (guides, `PLUGIN_ABI.md`, site page, `INSTALL_ARTIFACTS.md`) |
| FR-029, FR-045 | T075, T099, T114, T118 | `ctest -L architecture` (21 tests), `PluginAbi_RegistryConsistency` |
| FR-030 | T031, every checkpoint | the visual, shading-parity and determinism suites, `DisplayAbi_PixelsIdentical`, `ProceduresAbi_Scenes` |
| FR-031 | T013, T041, T047 | `ShadeopAbi_SignatureMatches` |
| FR-036 | T018, T108a, T109, T111, T115 | `PluginAbi_DocPaths` |
| FR-040 | T022, T083 | `DisplayAbi_UnsupportedParamType` |
| FR-042 | T035, T040, T052, T079 | `PluginAbi_VersionCheck`, the `*_Versions` tests (one message across two loads) |
| FR-047 | T037, T039, T085, T108a, T115 | `PluginAbi_VersionHistory`, `PluginAbi_DocPaths` (version comparison) |

| Success criterion | Tasks | Proving test or check |
|---|---|---|
| SC-001, SC-002 | T052, T064, T079 | `DisplayAbi_Versions_Reject/_V1/_V2`, `ShadeopAbi_Versions_Render/_Registry`, `ProceduresAbi_Versions` |
| SC-003 | T036, T047 | `PluginAbi_HeadersC17`, `PluginAbi_HeadersCxx20`, `DisplayAbi_HiddenVisibility` |
| SC-004 | T023-T025, T031e, T094, T125 | `DisplayAbi_PixelsIdentical` (macOS and Linux) |
| SC-005, SC-006 | T079, T080, T093, T120 | `DisplayAbi_TwoDisplayParams`, `DisplayAbi_Hostile`, `DisplayAbi_LateDisable`; the thread sanitizer run (see the finding on `hiderFlags` in section 2) |
| SC-007 | T064, T076 | `ShadeopAbi_PerThread`, `_ThreadLifetime`, `_ScanOrder`, `_LegacyShared` |
| SC-008 | T037, T053, T093 | `PluginAbi_SingleCheckRoutine`, `ShadeopAbi_ParserSingleDefiner`, `ProceduresAbi_NoLoadOutsideModule` |
| SC-009, SC-015 | T075, T099, T114, T118 | `ctest -L architecture`, `PluginAbi_RegistryConsistency`, the archcheck report (`METRIC` equal to the baseline) |
| SC-010 | T034, T047 | `PluginAbi_Convention` |
| SC-011, SC-019 | T102-T107, T121, T124 | `PluginAbi_InstallHeaders`, `_InstallList`, `_InstallLayouts`, `_ShadeopSoversion`; the real installs (section 6 and section 7) |
| SC-012 | T031a-T031e, T117-T119, T122-T125 | the full suites on both platforms (958 of 959 on macOS; 952 of 960 on Linux with the 7 baseline failures) |
| SC-013, SC-017 | T081, T094 | `DisplayV1_*`, `DisplayAbi_LegacyPixelsIdentical` |
| SC-014 | T013-T022, T030 | the tests of FR-031 to FR-040 above, each red on the starting tree (section 2) |
| SC-016, SC-020 | T108a, T108b, T111, T115 | `PluginAbi_DocPaths`, `PluginAbi_RegistryConsistency`, the site page build |
| SC-018 | T053, T061 | `ProceduresAbi_ConcurrentAcquire`, `ProceduresAbi_ObjectAndFrames` |

## 7. Linux results (T031a to T031e)

Host `juvenal@ubuntu-2404-desktop.v2-labs.io` (Ubuntu 24.04, GCC 13.3.0, cmake 3.28.3, 2 cores, 3.8 GiB RAM), new tree `~/Projects/Development/CLI/openrender-037`; the trees `openrender`, `openrender-028`, `openrender-039` and `openrender-fix111` were not touched. Every build and test ran through the remote `cpujob` at `-j2` (`CPUJOB_TIMEOUT=7200`).

**T031a preflight.** Free on `/home`: 8.8 GiB at the start. Expected build size: the macOS `build` is 596 MiB (`du -sh build`, vcpkg libraries live outside the tree); the Linux build uses system packages and measured 101 MiB for the test and library objects at the time of the check, plus about 100 MiB of sources, so about 1 GiB was expected against the 2 GiB margin required: 8.6 GiB were free after the build and the captures. (The first connection attempt failed at the DNS lookup of the host name; the network came back and the gate ran from there.)

**T031b sync.** `git ls-files -z --cached --others --exclude-standard` (3144 files, no `build*`, no `.git`) synced with `rsync -rlpc --files-from`; the sha256 manifest copied to the remote and `sha256sum -c` run there: **3144 OK, 0 mismatches**. The later re-sync is recorded at the end of this section.

**T031c build.** `cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DINSTALL_SELFCONTAINED=OFF` (cache confirmed: `OR037_RECAPTURE=OFF`, `INSTALL_SELFCONTAINED=OFF`, `CMAKE_BUILD_TYPE=Release`, JIT ON), then `cmake --build build -j2`: clean under GCC 13 `-Werror`, **0 warnings, no source change needed**. (The host's disk was the slow part: the configure took 12 minutes of I/O wait.)

**T031d baselines and red checks.**

| Run | Result |
|---|---|
| `ctest -LE 'determinism-sweep\|plugin_abi' -j2` | 896 tests: **888 passed, 7 failed, 1 skipped**, 639 s. The seven failures are exactly the known pre-existing Linux failures: `LibShader_McompConsistency` (#44), `TextureTile_RenderByteIdentical` (#45), `TextureTile_CacheByteIdentical`, `TextureTile_CacheDefaultDirectory`, `TextureTile_CacheStaleDetection`, `TextureTile_CacheConcurrentWrite` (#46), `CoverageAnalytic_alpha-sides2-tjunction-r4_reyes` (#57). Skipped: `LibShader_Format_rslo_lower_minor`. Saved: `tools/baseline-pass-list-linux.txt` (888 names). The plugin tests were excluded because the macOS baseline was taken before they existed |
| `ctest -L architecture` | 21/21; report `tools/archcheck-base-linux.txt`: `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0` |
| `ctest -L plugin_abi -j2`, twice, against the macOS references | 18 tests: 6 passed, 12 failed: the same **ten defect checks red for the same reasons** as on macOS, plus `DisplayAbi_PixelsIdentical` and `DisplayAbi_LegacyPixelsIdentical` (platform references, see T031e) |

Linux differences in the defect evidence: **FR-032** ends in `*** buffer overflow detected ***: terminated` (the process aborts, `Subprocess aborted`) where macOS prints `Failed to load DLO "bare_name"` and exits 255: the failing-load path of `Geometry "dlo"` aborts under glibc's fortified libc on Linux. The check stays red for the stated reason (the module's log has no `dloInit`) and the abort is pre-existing, not fixed here. **FR-033**: no signal and no call on Linux either; same single red assertion (no diagnostic names the symbol). All other reds are the same text as macOS (FR-038 reads `common/os.h: No such file or directory`).

**T031e references.** The macOS references do **not** pass on Linux: `DisplayAbi_PixelsIdentical` and `DisplayAbi_LegacyPixelsIdentical` fail in the floating-point and 16-bit scenes, and are identical in the others. Measured, per scene, Linux against macOS:

| Scene | Differing samples | Largest absolute difference |
|---|---|---|
| `tiff16` | 12 of 4096 | 1 (16-bit step) |
| `tifffloat` | 1588 of 4096 | 1.19e-05 |
| `png16` | 12 of 4096 | 256 (one step of the high byte in 16 bits) |
| `exrhalf` | 5 of 4096 | 0.015625 (one half-float step) |
| `depth` | 388 of 1024 | 2.62e-06 |
| `quantline` | 12 of 4096 | 1 |
| `tiff8`, `png8`, `rgbe`, `aov`, `dither`, `typetiff`, `notype`, `gamma` (pixels and gAMA 220000), `dlo_located`, `implicit_located`, `dynamicload_plain` | 0 | 0 |

The compare was not widened. `plugin-abi-capture-baselines` was run on the remote (it wrote `reference/linux/`, ran the frozen legacy driver against it - **all 11 file-driver scenes identical to the Linux live output** - and then refused to overwrite `scenes/reference/missing_symbol.txt`, which exists and is platform independent, so the target ends with exit 2 as designed). A second capture, `plugin-abi-capture-baselines-scratch`, is **identical sample for sample** to `reference/linux/` (`diff -r`), and its `missing_symbol.txt` and three Texture3d reports are byte-identical to the committed macOS ones. `tests/plugins/display_baseline/reference/linux/` (18 files, 167 KiB) was copied back into the worktree. Re-run on Linux, twice: **8 passed, 10 failed** (the ten defect checks); both pixel tests pass against `reference/linux/`.

### Phase 2 Linux gate

Re-synced `openrender-037` (3178 files, `sha256sum -c` on the remote: all OK, no mismatch) and rebuilt it with the remote `cpujob` at `-j2`: clean under GCC 13 `-Werror`, **0 warnings** (the cache reads `INSTALL_SELFCONTAINED=OFF`, `OPENRENDER_ENABLE_JIT=ON`, `OPENRENDER_TSAN=OFF`, `OR037_RECAPTURE=OFF`).

| Run | Result |
|---|---|
| `ctest -LE 'determinism-sweep\|plugin_abi' -j2` | 896 tests, 888 passed, 7 failed, 1 skipped: **exactly the seven known pre-existing Linux failures** of the Phase 1 baseline (#44, #45, #46 x4, #57); no new failure |
| `ctest -L architecture` | 21/21 |
| `ctest -L plugin_abi -j2`, twice, final code | 23 run: **14 passed, 9 failed** (the same nine defect checks as macOS); `PluginAbi_Convention`, `PluginAbi_VersionCheck`, `PluginAbi_HeadersC17`, `PluginAbi_HeadersCxx20`, `PluginAbi_VersionHistory` and `ShadeopAbi_SignatureMatches` pass on Linux; both pixel tests pass against `reference/linux/` |

**Found by the Linux gate.** The first Phase 2 run on Linux aborted `PluginAbi_Convention` and `PluginAbi_VersionCheck` with `*** buffer overflow detected ***: terminated`. Cause, in my code: `checkPluginVersion` canonicalised the cache key with `realpath(path, buf)` into an `OS_MAX_PATH_LENGTH` buffer; glibc's fortified `realpath` aborts when the buffer is smaller than `PATH_MAX`. Fixed in `pluginAbi.cpp` (`realpath(path, nullptr)`, freed), then re-synced and re-run as above. **The same defect is in `osLoadModule` (`src/common/os.cpp`, the retry with `realpath(name, absoluteName[OS_MAX_PATH_LENGTH])`)** and explains the Linux abort of `ProceduresAbi_BareNameLocated` in the Phase 1 evidence: every failing first `dlopen` ends in the abort on Linux instead of returning NULL. Not fixed here (outside the spec's files); for the maintainer.

### Phase 3 Linux gate

`openrender-037` re-synced (files deleted or moved locally were removed on the remote too, so no stale `src/includes/`, `src/common/orplugin.h` or `src/ri/geometry/dlo.h` could shadow the new ones; `sha256sum -c`: all 3190 files OK), rebuilt at `-j2`: clean under GCC 13 `-Werror`, 0 warnings. `ctest -L plugin_abi -j2` twice: 29 run, **21 passed, 8 failed** (the same eight as macOS: the defect checks of Phases 4, 5 and 7 and the doc check; `PluginAbi_SingleCheckRoutine` disabled). On Linux `PluginAbi_LoadMissingModule` and `ProceduresAbi_BareNameLocated` pass: the `osLoadModule` abort is gone. `ctest -L architecture` 21/21. `ctest -LE 'determinism-sweep|plugin_abi' -j2`: 896 tests, 7 failed, 1 skipped, **exactly the seven known pre-existing Linux failures**. The `*** buffer overflow detected ***` lines that remain in that log (8, the same count as before Phase 3) come from `TextureTile_CacheConcurrentWrite` (#46), not from `osLoadModule`: a second, separate abort on Linux for the maintainer's information.

**Final re-sync and tree check** (T033). After the last edit the file list was regenerated (3165 files: the Phase 1 tree plus `reference/linux/`, the two Linux tool reports and this file), re-synced with `rsync -rlpc --files-from`, the manifest copied to the remote and `sha256sum -c manifest.sha256` run there: all 3165 files OK. The remote tree `openrender-037` holds the Phase 1 state it was built from and is left in place (with its build directory) for the final-phase Linux validation.

### Phase 8 Linux gate (T122 to T125)

**Re-sync (T122).** `openrender-037` re-synced from the staged tree (`git ls-files -z --cached --others --exclude-standard`, `rsync -rlpc --files-from`, then a fresh local `sha256` manifest copied to the remote and `sha256sum -c` there: every file OK). No file was deleted by the branch since the previous sync (the file lists under `src tests CMake examples docs DEVNOTES_DETAILS specs shaders` have 2825 entries on both sides). Remote disk: 9.7 GB free before the build. The remote cache still has `INSTALL_SELFCONTAINED=OFF`, `CMAKE_BUILD_TYPE=Release`, `OPENRENDER_TSAN=OFF`.

**Build and tests (T123).** GCC 13 `-Werror`, `-j2` through the remote `cpujob`: configure and build clean, 0 warnings. `ctest -LE determinism-sweep -j2`: 960 registered, **952 passed, 7 failed, 1 skipped**; the seven failures are exactly the Linux baseline (`LibShader_McompConsistency`, `TextureTile_RenderByteIdentical`, `TextureTile_CacheByteIdentical`, `_CacheDefaultDirectory`, `_CacheStaleDetection`, `_CacheConcurrentWrite`, `CoverageAnalytic_alpha-sides2-tjunction-r4_reyes`), the skip is `LibShader_Format_rslo_lower_minor`; all of `PluginAbi_DocPaths`, `_RegistryConsistency`, `_InstallHeaders`, `_InstallList`, `_InstallLayouts` (70 s) pass. `ctest -L plugin_abi -j2`: 64/64. `ctest -L architecture`: 21/21; the full report: `ALLOWED` unchanged, `METRIC TOTAL refs 651 (baseline 651) distinct 97 (baseline 97)`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`. `-L '^determinism$' -j2`: 155/155 (355 s). `-L '^determinism-sweep$' -j2`: 419/419 (1382 s).

**Real install, FHS layout (T124).** `DESTDIR=$HOME/openrender-037-stage cmake --install build` through the remote `cpujob`: exit 0. `file_base.h` is absent; the include directory is `usr/local/include/openRender` and holds the nine headers; `libRIBShadeop.so.1.0.0` (SONAME `libRIBShadeop.so.1`, like `libRIBCommon.so.1`) and `libRIBShadeop.a` are installed; the package files are in `usr/local/lib/cmake/openRender`. `examples/display/simpledisplay.c` builds against that directory alone (`-std=c17 -Wall -Wextra -Wpedantic -Werror -fPIC -shared`), and the staged `orender` (remote `cpujob 037-lx-render`, `SHADERS` and `DISPLAYS` of the staged tree) renders `examples/rib/camera-dof.rib` through it into a 400 x 300 P6 file. Nothing landed outside the staging directory: the mtimes of `/usr/local`, `/usr/local/bin`, `/usr/local/include` and `/usr/local/lib` are unchanged and `/usr/local/include` is empty. The staging directories were deleted afterwards.

**Pixel references on the final Linux tree (T125).** `DisplayAbi_PixelsIdentical` (includes the PNG `gAMA` check), `DisplayAbi_LegacyPixelsIdentical` and `ProceduresAbi_Scenes` pass in the run above against the Phase 1 references; zero differing samples, nothing regenerated, no comparison widened.
