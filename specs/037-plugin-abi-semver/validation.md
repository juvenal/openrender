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

## 5. Architecture reports and metrics

| Checkpoint | `ctest -L architecture` | Report |
|---|---|---|
| Starting tree (T004) | 21/21 | `tools/archcheck-base-macos.txt` |
| Phase 1 (T031 CP-c) | 21/21 | identical to the starting-tree report (`diff` empty); `tests/plugins` produced no `unclassified:` line; no `STALE`, no `VIOLATION` |
| Phase 2 (T048 CP-c) | 21/21 | `ALLOWED` line identical (no allowlist line added or removed), `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; `METRIC` lines identical; the object count goes 279 -> 280 (`pluginAbi.cpp`) and the include edges 1534 -> 1538 (the four family headers include `orplugin.h`, all `* -> Infra`), and one informational line is added: `UNRESOLVED src/common/orplugin.hpp (no compiled translation unit includes it)`, like the ten such header lines already in the base report. No `unclassified:` (`prefix src/common/ Infra` covers the new files) |

## 6. macOS final pass

Not yet (final phase).

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
