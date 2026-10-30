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

**Final re-sync and tree check** (T033). After the last edit the file list was regenerated (3165 files: the Phase 1 tree plus `reference/linux/`, the two Linux tool reports and this file), re-synced with `rsync -rlpc --files-from`, the manifest copied to the remote and `sha256sum -c manifest.sha256` run there: all 3165 files OK. The remote tree `openrender-037` holds the Phase 1 state it was built from and is left in place (with its build directory) for the final-phase Linux validation.
