# Validation record: spec 030, Hider and Integrator Registry

Worktree `/Volumes/Projects/Development/CLI/openrender-worktrees/030-hider-and-integrator-registry`, branch
`030-hider-and-integrator-registry`. Base for every `git diff <base>`: master `e83d005d`.

## §0 Starting-tree measurements and orientation

### T001 / T002 orientation (2026-11-09)

- T002 (PAUSE 1a) confirmed by the maintainer 2026-11-09: the Phase 1 file list, departures D1-D5 and gaps G1-G7 as written.
- `git log -1` at the start: `b467daec` (branch `030-hider-and-integrator-registry`, based on master `e83d005d`); `git status --short`: clean.
- CLAUDE.md "Spec-kit agent rules": both copies (this worktree and the main checkout) carry the section; the two files are
  byte-identical (`diff` empty), same last-commit time (1793933858). The worktree copy was followed; the rules are identical.
- Environment given by the coordinator: `VCPKG_ROOT=/Users/juvenal/.vcpkg`,
  `VCPKG_INSTALLED_DIR=/Users/juvenal/.cache/vcpkg-installed/openrender`; the configure runs from the `/Volumes` worktree path.
- Disk (T001): `df -h /Volumes/Projects` **6.3 GiB free** at the start (the tasks stage expected about 8.6 GiB; three other
  specs are building in parallel worktrees). Above the 2 GB stop line.
- SC-010 timing scenes (T004): **`teapot-somewood-reyes`** (reyes hider, shading-heavy) and **`killeroo-raytrace`** (raytrace hider).
  Scaling factors in `tools/timing_sc010.sh` are the initial choice (format 3, shadingrate 2 each); the first run flags
  `TOO SHORT` if they need raising, and any change is recorded here.

### macOS baseline (T006 to T011; starting `src/` at `e83d005d`, Release, JIT on, vcpkg shared tree, `-Werror`)

Environment: `VCPKG_ROOT=/Users/juvenal/.vcpkg`, `VCPKG_INSTALLED_DIR=/Users/juvenal/.cache/vcpkg-installed/openrender`; configure and build from the `/Volumes` worktree path. The machine-wide `cpujob` lock was contended by three other specs, so wall times include queueing.

| Task | Result |
|---|---|
| T006 configure (`030-base-configure`) | exit 0 |
| T006 build (`030-base-build`, `-j4`) | exit 0; every target built; 0 compiler warnings (`grep 'warning:'` finds only 23 `ld: warning: ignoring duplicate libraries` lines from the vcpkg static libraries, as on 029's baseline); `build` is 577 MB |
| T007 suite (`030-base-suite`, `-LE determinism-sweep -j4`) | **1097 tests, 100% passed**; 1 test skips itself on the baseline (`LibShader_Format_rslo_lower_minor`, `Skipped`). Real time 349 s. `baseline-images-macos.sha256`: **915 images**; `baseline-diffs-macos.txt`: 389 tests with a `MaxBlockAvgDiff` |
| T008 noise (`030-base-suite2`) | run 2: 1097 tests, 100% passed, same skip (276 s); **915 images, 915 fresh, 0 differing, 0 missing**; every `MaxBlockAvgDiff` equal. `baseline-image-noise-macos.txt` is empty (comment only) |
| T009 archcheck (`030-base-archreport`) | `ALLOWED DB-004 x 52, DB-006 x 30, DB-010 x 8, DB-011 x 164, DB-017 x 78, DB-025 x 12`; `METRIC TOTAL refs 604 distinct 89` (Geometry 48/15, Hiders 556/81, the other classes 0, all equal to `metrics-baseline.txt`); `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`. `ctest -L architecture`: **29 of 29 passed**. DB-011 starts at 164 lines, as the tasks expected |
| T010 sweep (`030-base-sweep1`, `-L '^determinism-sweep$' -j2`) | **419 of 419 passed**, 572 s; 419 images in `baseline-sweep-images-macos.sha256` (the sweep leaves them under `build/tests/visual/determinism-sweep`, so the manifest mechanism applies as it is) |
| T010 sweep noise (`030-base-sweep2`) | run 2: 419 of 419 passed (516 s); the two manifests are identical (`diff` empty): **0 differing**; `baseline-sweep-noise-macos.txt` is empty |
| T011 frozen copy (`build-base`) | `cp -Rp build build-base` (577 MB) under `cpujob 030-freeze-base`, then install names, `LC_RPATH` entries and dylib ids rewritten with `install_name_tool` and every modified Mach-O re-signed (`codesign -f -s -`) by a scratch script (not part of the spec's tools): **464 Mach-O files seen, 217 rewritten, 0 still naming `$W/build/` in `otool -L` or `LC_RPATH`**. Run check (`030-frozen-check`): one `orender` run of `teapot-matte-reyes` from `build-base` with `DYLD_PRINT_LIBRARIES=1` loads 5 openRender images (`libRIBRender`, `libRIBShadeop`, `libRIBCommon`, `libshader_shading`, `file.dsply`) plus `orender`, **all from `build-base/`, none from `build/`**; exit 0, the TIFF was written. `build-base` is never rebuilt and never edited |

Disk during the macOS baseline: 6.3 GiB at the start, then wildly variable because three other specs build in parallel (it fell to 0.45 GiB for a moment during another spec's job, and was freed by the maintainer to 8.6 GiB); 7.5 GiB free after T011 and the sweeps. Every heavy job of this phase was started with at least 2.3 GiB free.

### Phase 2 drafting stage (T021 to T023, before the TEST-APPROVAL; 2026-11-09)

Drafts live in `tools/test-drafts/` (not compiled by the build, not registered, not under `tests/`); nothing under `tests/` or `src/` has been touched yet.

- **What was checked on the drafts without applying them** (syntax-only compiles with the tree's own flags, `-Werror`, through `cpujob`, nothing run; and two `cmake -P` runs):
  - `test_registry_hider_statements.cpp` and `test_hcshader_missing_shader.cpp` compile clean against the starting tree (they use RI calls and existing headers only).
  - The rest compile clean against **stub headers written from the contracts** (`hiderAbi.h/.hpp`, `hiderRegistry.h`, `hiderPeers.h`, `transport/{capability,capabilityCheck,integratorAbi,integratorRegistry,classicIntegrator}.h` and a copy of `options.h` with the integrator members): this checks the drafts against the contracts, not against the future code. The only errors left are the expected one, `RiIntegrator` undeclared (Phase 7 adds it).
  - `check_hcshader_references.cmake` and `check_core_names_no_hider.cmake` were run on the starting tree: both fail for the stated reason. The first lists exactly the FR-025 places (`shaderRefs.h`, `shading.h`, `src/ri/CMakeLists.txt` x5, `hcshader.{h,cpp}`, `renderer.cpp`, `rendererContext.cpp` x3, `rendererStatics.cpp`, `tests/architecture/domain-map.txt`) and none of the exempt files; the second lists the five `#include`s, the `C*::preDisplaySetup` calls and the `strcmp(hider` chains of `renderer.cpp`.
  - The two helper scripts were run against a scratch copy of the helpers with the one-line `Integrator` method added (pass) and against the unpatched helpers (fail with `AttributeError` / a nil call): macOS Python 3.15.2 and Lua 5.5.1. Linux (remote `cpujob`, scratch copy in `/tmp`, removed): Lua 5.4.6 and Python 3.12.3, both pass against the patched helpers.
- **Interface assumptions the drafts make that the contracts do not spell out** (each is a point the later phases must meet or the maintainer must change in the draft): see the T023 report; they are listed in §4 F7.

## §1 Checkpoint records

(one table per phase: CP-a to CP-d, `ALLOWED`/`METRIC`/`RESULT`; none yet)

### Phase 2 checkpoint (T031, T032; no `src/` change)

| Check | Result |
|---|---|
| CP-a build (option OFF, `030-p2-build`, `-j4`) | exit 0, 0 warnings |
| CP-b suite (`030-p2-suite`, `-LE determinism-sweep -j4`) | **1103 registered, 1099 run, 100% passed**, 232.8 s; the 4 other tests are the registered-but-`DISABLED` script and helper tests (`Not Run (Disabled)`: `Helpers_PrmanIntegratorPython`, `Helpers_PrmanIntegratorLua`, `Hcshader_NoReferences`, `Registry_CoreNamesNoHider`); 1 test skips itself as on the baseline. Equals T007's pass set (1097) plus the two characterization tests |
| CP-d images | `image_manifest.sh diff`: manifest 915 images, fresh 920, **0 differing, 0 missing**, 5 not in manifest (the new tests' own images under `tests/unit/registry/work/`, information only); `diffs` equal to `baseline-diffs-macos.txt` (empty diff) |
| CP-c architecture (`030-p2-arch`) | `ctest -L architecture`: 29 of 29 passed (the two new script tests are `Not Run (Disabled)`); full report `ALLOWED`/`METRIC`/`RESULT` lines identical to T009 (`DB-011 x 164`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`) |

## §2 Red evidence

### Phase 2 (T028 to T030; starting `src/`, generator Unix Makefiles on macOS; logs `build-logs/030-p2-*.log`)

**Characterization tests, green on the starting tree (T028)**, built with the option OFF (`030-p2-build`: exit 0, 0 warnings) and run through ctest (`030-p2-char`, `-V` in `030-p2-char-v.log`): `Registry_HiderAliasDiagnostics` Passed (0.03 s) and `Registry_UnknownHiderFallback` Passed (0.19 s). What they pin, as the run printed it:
- `Deprecated Hider "stochastic"; use "reyes" instead` once per `stochastic` statement (the API session printed it twice for two statements, the frame with two render threads once);
- `Hider "bogus" unavailable` once per frame (`./unknown_bogus.rib (10)`), the reyes image byte-identical to `Hider "reyes"`, `RiLastError` set; with a TSM display both `Hider "bogus" unavailable` and `Hider must be reyes (or hidden, or the deprecated stochastic alias) for TSM` appear.

**Compiled tests, red evidence (T029)**: `cmake -B build -S . -DOPENRENDER_030_RED_TESTS=ON` (`030-p2-red-configure`, exit 0), then the eleven new targets with `-- -k` (`030-p2-red-build`, exit 2, log `build-logs/030-p2-red-build.log`). Each target, and the first error that makes it red:

| Target (ctest names) | Result | First error |
|---|---|---|
| `test_registry_integrator_table` (Registry_IntegratorLookup, Registry_PairCheck) | build failure | `'transport/capability.h' file not found` |
| `test_registry_hider_table` (Registry_HiderLookup, Registry_HiderCollisions) | build failure | `'hiderAbi.h' file not found` |
| `test_registry_hider_double` (Registry_TestDoubleHider) | build failure | `'hiderRegistry.h' file not found` (and `'hiderAbi.hpp'` in `hider_double.cpp`) |
| `test_registry_pairs` (Registry_PairRefusal_x4, Registry_ClassicPairs) | build failure | `'transport/classicIntegrator.h' file not found` (and `transport/capability.h` in `integrator_doubles.cpp`) |
| `test_registry_integrator_statements` (Registry_UnknownIntegratorFallback, Registry_IntegratorParameters) | build failure | `use of undeclared identifier 'RiIntegrator'` (plus the doubles' missing headers) |
| `test_registry_frame_abort` (Registry_FrameAbort) | build failure | its two dependencies `hider_double.cpp` and `integrator_doubles.cpp` (`hiderAbi.hpp`, `transport/capability.h` not found) |
| `test_integrator_statement` (RibParse_IntegratorStatement) | build failure | `no member named 'integrator' in 'COptions'` (19 errors) and `RiIntegrator` undeclared |
| `test_scene_interface_integrator_roundtrip` (RibOut_IntegratorRoundTrip) | build failure | `no member named 'integrator' in 'COptions'` (4 errors) |
| `test_link_static_consumer_registries` (Link_StaticConsumerRegistries) | build failure | `'hiderRegistry.h' file not found` |
| `test_hcshader_missing_shader` (Hcshader_MissingShader) | **builds** (finding F8) | red by assertion, below |
| `test_registry_identity` (Registry_IntegratorImageIdentity) | **builds** (finding F8) | red by assertion, below |

Then the option was reconfigured OFF (`030-p2-off`): configure and build exit 0, 0 warnings, `git status` shows no tracked change from the reconfigure.

**Script and runtime tests, red evidence (T030)**, in the option-ON configure (`030-p2-red-scripts`, `build-logs/030-p2-red-scripts.log`): `ctest -R 'Hcshader_NoReferences|Registry_CoreNamesNoHider|Hcshader_MissingShader|Registry_IntegratorImageIdentity|Helpers_PrmanIntegrator'`: **0 of 6 passed**, all six red for the stated reason:
- `Hcshader_NoReferences` Failed: lists `src/libshader/shading/shaderRefs.h` (1), `shading.h` (1), `src/ri/CMakeLists.txt` (5), `src/ri/hcshader.cpp` (40), `hcshader.h` (11), `renderer.cpp`, `rendererContext.cpp`, `rendererStatics.cpp`, `tests/architecture/domain-map.txt`; no registration line of the new CMake files and no exempt file.
- `Registry_CoreNamesNoHider` Failed: the five hider `#include`s, the `C*::preDisplaySetup()` calls and the `strcmp(hider` chains in `renderer.cpp`, `rendererContext.cpp`, `rendererDisplay.cpp`, `rendererStatics.cpp`.
- `Hcshader_MissingShader` Failed by assertion: `CHECK failed: s.frames.count("Failed to find shader \"spherelight\"") == 1` (and `quadlight`): the C++ substitute answers, the only `Failed to find shader` reports are for `defaultsurface` and `defaultlight` (an empty search path).
- `Registry_IntegratorImageIdentity` Failed by assertion: `Integrator ... RIB Parse error` for every hider and `CHECK failed: classic == none1` (the controls, two renders with no statement, pass).
- `Helpers_PrmanIntegratorPython` Failed: `AttributeError: 'Ri' object has no attribute 'Integrator'`; `Helpers_PrmanIntegratorLua` Failed: `attempt to call a nil value (method 'Integrator')`.

## §3 Linux records

Host `juvenal@ubuntu-2404-desktop.v2-labs.io` (GCC 13.3.0, Ubuntu 24.04, 2 cores, LLVM 18, Python 3.12.3; no Lua at the time of the baseline, Lua 5.4.6 installed afterwards, see F4), per-spec tree `~/Projects/Development/CLI/openrender-030`, remote `cpujob` at `-j2` (`CPUJOB_TIMEOUT=7200` for the first build and the suites; the remote lock was shared with spec 031). The host was unreachable ("Network is unreachable") from the start of the phase until the coordinator reported it back; T012 to T015 ran after that. `openrender-030` was checked absent before `mkdir -p`; remote disk 9.8 GiB free at T005, 9.1 GiB after the sweeps.

| Task | Result |
|---|---|
| T005 push + verify | `linux_sync.sh push`: 3727 files; `verify`: **3727 files, 0 differences** (final push after the macOS baseline files were staged: 3729 files, 0 differences) |
| T012 configure + build (`030-linux-base-build`, `-j2`, Release, JIT on via LLVM 18) | exit 0, **0 `warning:` lines** under `-Werror` |
| T013 suite (`030-linux-base-suite`, `-LE determinism-sweep -j2`) | **1098 tests, 1097 passed, 1 failed**: `TextureTile_RenderByteIdentical` (see T016). 1 test skips itself (`LibShader_Format_rslo_lower_minor`). 701 s. 1098 against macOS's 1097: one test exists only on Linux. `baseline-images-linux.sha256`: **915 images**; `baseline-diffs-linux.txt`: 389 tests with a `MaxBlockAvgDiff` (same count as macOS) |
| T014 noise (`030-linux-base-suite2`) | run 2: same 1097 passed, same failure and skip (679 s); **915 images, 0 differing**; every `MaxBlockAvgDiff` equal. `baseline-image-noise-linux.txt` is empty (comment only) |
| T014 archcheck | `ALLOWED` identical to macOS (`DB-004 x 52, DB-006 x 30, DB-010 x 8, DB-011 x 164, DB-017 x 78, DB-025 x 12`), `METRIC TOTAL refs 604 distinct 89`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; `ctest -L architecture`: **29 of 29 passed** |
| T015 sweep (`030-linux-base-sweep1`, `-L '^determinism-sweep$' -j2`) | **418 of 419 passed (1404 s)**: `DeterminismSweep_sphere-texture3d-bake3d-reyes` crashed (finding F2). 419 images in `baseline-sweep-images-linux.sha256` |
| T015 sweep run 2 (`030-linux-base-sweep2`) | **419 of 419 passed (1338 s)**; the two manifests differ in exactly one path, the image of the crashed test (`baseline-sweep-noise-linux.txt` lists it) |
| pull + verify | `linux_sync.sh pull`: the 8 Linux-produced files, 8 files, 0 differences |

### Phase 2 light CP-L (T033; `linux_sync.sh push` 3795 files, `verify` 0 differences; remote `cpujob` at `-j2`, default timeout)

Build with the option OFF (`030-linux-p2`: configure + build, exit 0, 0 `warning:` lines under GCC 13 `-Werror`); `Registry_HiderAliasDiagnostics` and `Registry_UnknownHiderFallback` **Passed** (0.91 s, 0.03 s), so the pinned texts are the same on Linux; `ctest -L architecture`: 29 of 29 passed (the two new script tests `Not Run (Disabled)`). The Linux red evidence of the compiled tests is not repeated. The Lua draft ran on Lua 5.4.6 and Python 3.12.3 against the patched helpers (see the drafting-stage note in §0).

### T016: the Linux starting failure set

The only Linux non-pass set of the starting tree (the only allowed Linux non-pass set for the whole spec, plus the self-skip):

1. `TextureTile_RenderByteIdentical` (fails in both suite runs): `texture-png-reyes`, `texture-exr-reyes`, `texture-exr-raytrace`, `texture-rgbe-reyes`, `env-reyes` and `env-raytrace` are not byte-identical to `tests/unit/texture_tile/fixtures/references/*.tif` (`differ: byte 5, line 1`, the TIFF header); the png-raytrace, rgbe-raytrace and both shadow renders are identical. The references are macOS captures: this is the per-platform reference issue the maintainer holds until spec 042 (memory "#45 on hold"), not caused by 030.
2. `LibShader_Format_rslo_lower_minor`: skips itself (as on macOS).
3. Intermittent (not in the set, a hazard): `DeterminismSweep_sphere-texture3d-bake3d-reyes` crashed once in two sweeps (F2).

## §4 Findings and departures from the plan

- **F1 (plan claim (a) of T017 is false: four approved tests set `HIDER_ILLUMINATIONHOOK` through `CRenderer::hiderFlags`).** `tests/shading_parity/test_false_if_constructs.cpp:127-151`, `test_illumination_hook.cpp:217-241`, `test_illumination_hook_conditions.cpp:205-221` and `test_illumination_hook_operands.cpp:224-240` each do `CRenderer::hiderFlags |= HIDER_ILLUMINATIONHOOK` (and restore it). T110 deletes the constant and contracts/frame-transport.md moves `rendererServicesImpl.h:61` to `frameTransport().traits.illuminationHook`. No task names these tests and "Edits to existing tests: none" forbids touching them, so Phase 8 as written breaks their build (or, if the constant stayed, the hook would no longer follow `hiderFlags`). Needs a maintainer decision before Phase 8; detail in `tools/test-edit-inventory.txt` (a). No other test, and nothing in `src/python`, `src/lua`, `src/preview` or `src/orender`, names a concrete hider, `hiderFlags`, `HIDER_*` or `preDisplaySetup`.
- **F2 (GitHub #122; intermittent crash on the starting tree, Linux; outside this spec, for the coordinator).** Already filed as #122 (intermittent `CPointCloud::lookup` segfault in this scene on Linux); the coordinator added this occurrence there. `DeterminismSweep_sphere-texture3d-bake3d-reyes` in sweep run 1 on Linux: "Configuration 2 of 5 (default): ... Segmentation fault (core dumped), orender failed with exit code 35584". It passed in sweep run 2 and in both macOS sweeps; not reproduced further (no debugger is allowed; no core analysis). Spec 030 must not be blamed for it at a later Linux checkpoint: it is in the baseline.
- **F3 (T017 (b), (c), (d) measured).** G2's list of `hcshader` mentions is complete and adds `src/libshader/shading/shaderRefs.h:41` (comment), `tests/architecture/domain-map.txt:69` and `tests/architecture/unit/test_archcheck_parsers.cpp:104,130-131` (in-memory data, exempt by Q8). The class count is data only (`kClassCount = 12` plus `kClassNames`); a rule table does not need a row per class, so the 18 fixtures' own `domain-rules.txt` copies need no edit. `ribvector-core-residual.txt` names `src/ri/render/rendererStatics.cpp`, which T110 edits: its row must stay valid or be deleted in the same task.
- **F4 (T018; updated 2026-11-09).** Both hosts have `python3` (3.15.2 macOS, 3.12.3 Linux). Lua: macOS has `lua` 5.5.1; at the T018 measurement Linux had none, and the maintainer has since installed one there: **`lua` is Lua 5.4.6 on Linux (Ubuntu 24.04 ships no 5.5)**. The new Lua helper test (`tests/lua/test_prman_integrator.lua`) can therefore run on both hosts, and its code must work on **Lua 5.4** (the lower of the two: no 5.5-only syntax or library function). It stays guarded by `find_program`. The existing `tests/python/test_prman.py` and `tests/lua/test_prman.lua` are unregistered pure text-generating helpers; the Lua one needs `LUA_PATH='src/lua/?.lua;;'`.
- **F5 (build-base).** The frozen copy was made by a scratch script, not a spec tool (the recipe in 029's record is prose only); the numbers are in §0.
- **F7 (Phase 2 drafts: assumptions beyond the contracts).** (1) `CIntegratorRegistry` mirrors `CHiderRegistry` (`add(table, origin)`, `find(name)` returning `{table, alias}`, `size()`, `at(i)`, `integratorRegistry()`); (2) `transportClassicLifecycleCounts()` returns `SClassicLifecycleCounts { uint64_t configure, prepare, finish; }` by value; (3) a table refused for an alias collision is refused whole (nothing of it registered); (4) the diagnostics of the version check carry `CODE_VERSION` for both a MAJOR mismatch and a too-small `structSize`; (5) a `configure` failure destroys the integrator instance it created (the contract is silent: the draft asserts `destroyed == created`) and a `prepare` failure destroys both the hiders and the instance, without `finish`; (6) the pair-refusal text carries `Hider "<h>" cannot run integrator "<i>"`, `missing "<c>" <v>` or `"<c>" <v1> provided, <v2> required`, and `the frame is not rendered` (the contract's table); (7) the process hider registry holds exactly 4 entries in a program that links no other hider, 6 with `hider_double.cpp`, and the integrator registry 1 / 7 with `integrator_doubles.cpp`; (8) the double hider provides `display-output`, so a frame with a `Display` file driver runs it and the file driver writes an image of the (empty) samples; whether that driver tolerates a hider that submits no samples is first known at Phase 8; (9) `Registry_PairCheck` asserts the verdict's fields; the human-readable text with both versions is asserted in `Registry_PairRefusal_*`, where it is produced.
- **F8 (T029's premise).** Two new compiled tests compile on the starting tree and are red by assertion at run time, not by build failure: `Hcshader_MissingShader` and `Registry_IntegratorImageIdentity` (RI calls only). The red evidence of T029/T030 therefore shows a failing run for these two, and a build error for the others.
- **F6 (disk).** `/Volumes/Projects` free space swung between 0.45 GiB and 8.6 GiB during the phase because other specs build and test in parallel. Every heavy job of this phase started above the 2 GB floor; a dip to 1.4 GiB and one to 0.45 GiB happened during other specs' jobs while mine was queued or running (nothing of mine failed).

## §5 Final summary against SC-001 to SC-011

(at Phase 10)
