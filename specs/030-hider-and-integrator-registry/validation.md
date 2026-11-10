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

### Phase 3 checkpoint (T044 to T047; the governance gate; no `src/` change)

| Check | Result |
|---|---|
| Amendment applied (T038) | through the `speckit-constitution` workflow with `tools/constitution-1.4.0-draft.md` as input; the hooks were declined (the mandatory `before_constitution` `speckit.git.initialize`, because the repository exists, and the optional `after_constitution` commit). The resulting `git diff` of `.specify/memory/constitution.md` and `.specify/templates/plan-template.md` equals the draft's planned diff line for line (only the function-context text of three hunk headers differs). Constitution 1.4.0, Last Amended 2026-11-09, ten domains, the Sync Impact Report |
| Data files (T039, T040) | `domain-rules.txt`: header and Source line v1.4.0, `Transport -> GraphicsState Geometry Shading`, Hiders row `... Display Transport`; `domain-map.txt`: `prefix src/ri/transport/ Transport` (no other `src/ri/` prefix shadows it; the `src/ri/hcshader.` line stays for Phase 4). `metrics-baseline.txt` unchanged (T045: the report lists only the classes that use the core, no `Transport` row) |
| archcheck class (T043) | `config.h` (`Class::Transport` after `Display`, `kClassCount` 13), `config.cpp` (`kClassNames`, comment "the ten domains plus Infra"), `main.cpp` (help text) |
| CP-a build (`030-p3-build`) | exit 0, 0 warnings |
| T044 fixtures | `ctest -R ArchFixture`: **20 of 20 passed**, both new fixtures green: `transport-allowed` `RESULT PASS violations=0`; `transport-cycle` `VIOLATION include src/transport/classic.cpp:1 -> src/hiders/hider.h [Transport -> Hiders] rule: allowed-dependency table (Transport may depend on: GraphicsState, Geometry, Shading, Infra)` and `CYCLE Hiders -> Transport -> Hiders ... [FAILING]`, exit 1; every old fixture unchanged |
| Class-count dependence | none found: `Architecture_Parsers`, `Architecture_DomainBoundaries` and `Architecture_Determinism` pass unedited (`ctest -L architecture`: 31 registered, **31 of 31 passed**, the two new script tests `Not Run (Disabled)`); the full report's `ALLOWED`, `METRIC` and `RESULT` lines are identical to T009 (DB-011 x 164, TOTAL 604/89, `RESULT PASS`) |
| CP-b suite (`030-p3-suite`, `-LE determinism-sweep -j4`) | **1105 registered, 1101 run, 100% passed** (249.2 s) = Phase 2's 1099 plus the two fixtures; the 4 disabled tests as before; 1 self-skip |
| CP-d guard | `image_manifest.sh diff`: manifest 915, fresh 920, **0 differing, 0 missing**, 5 not in manifest (the Phase 2 tests' own images); `diffs` equal to `baseline-diffs-macos.txt` |
| T047 ordering gate | `src/ri/transport` does not exist; `git grep -n 'transport/' -- src` finds nothing; `git diff --cached --name-only -- src` is empty |

### Phase 4 checkpoint (T058 to T060; the `hcshader` retirement; macOS)

| Check | Result |
|---|---|
| Test gate (T050; `030-p4-configure`, `030-p4-tg-build`, `030-p4-tg-run`, `-R 'Hcshader_'`) | both tests enabled (see §2): `Hcshader_NoReferences` Failed and `Hcshader_MissingShader` Failed, each red for its stated reason, **before** any `src/` edit |
| CP-a build (`030-p4-build`, `-j4`) | exit 0; no compiler warning (`grep 'warning:'` finds 21 linker warnings only: 6 `ignoring duplicate libraries` and 15 about the `orender-wire-macos` Swift product and libribpreview objects built for macOS 13.3 and linked at 13.0; the latter are not new source warnings and are named here so the next phase can compare); `ribVector` builds, the residual check is green |
| T058 explicit set (`030-p4-lights`, `-R 'Visual_spherelight\|Visual_quadlight\|Sampling_ThreadSweep_quadlight\|Hcshader_'`) | **6 of 7 passed**: `Visual_spherelight`, `Visual_spherelight-slo`, `Visual_quadlight`, `Visual_quadlight-slo` (references and thresholds untouched), `Sampling_ThreadSweep_quadlight` and `Hcshader_NoReferences` pass; **`Hcshader_MissingShader` still fails, finding F9** (a defect in the approved test, not in the source) |
| CP-b suite (`030-p4-suite`, `-LE determinism-sweep -j4`) | **1103 run, 1102 passed, 1 failed** (210.7 s): the failure is `Hcshader_MissingShader` (F9). Not Run: `Helpers_PrmanIntegratorPython`/`Lua` and `Registry_CoreNamesNoHider` (still `DISABLED`) and the self-skip `LibShader_Format_rslo_lower_minor`. Phase 3's 1101 run plus the two Hcshader tests enabled by T050; nothing else changed state |
| CP-d images | `image_manifest.sh diff`: manifest 915 images, fresh 921, **0 differing, 0 missing**, 6 not in manifest (the Phase 2 and 4 tests' own images, information only); `diffs` equal to `baseline-diffs-macos.txt` (empty diff). The four spherelight/quadlight scenes (`-rslo` and `-slo`) render the same bytes as the baseline |
| CP-c architecture (`030-p4-arch`, `030-p4-archreport`) | `ctest -L architecture`: 33 registered, 32 run, **32 of 32 passed** (`Registry_CoreNamesNoHider` `Not Run (Disabled)`; `Hcshader_NoReferences` now green). Full report: `ALLOWED DB-004 x 52, DB-006 x 30, DB-010 x 8, DB-011 x 164, DB-017 x 78, DB-025 x 12`, `METRIC TOTAL refs 604 distinct 89`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; the `ALLOWED`/`METRIC`/`RESULT` lines equal `archcheck-base-macos.txt` line for line. `allowlist.txt`: 344 entries, untouched |

### Phase 4 checkpoint, second run after the approved F9 test fix (2026-11-10; macOS)

Maintainer approval (relayed by the coordinator, 2026-11-10) of two edits to approved tests, and nothing else: in `tests/unit/registry/test_hcshader_missing_shader.cpp` the comparison `r.code == CODE_NOSHADER` -> `r.code == RIE_NOSHADER` (:55; the message at :57 and the header comment at :11 follow), and in `tests/unit/registry/test_registry_frame_abort.cpp` `CODE_BUG` -> `RIE_BUG` (:75, message :77). Diffs (`git diff -U0`):

```
test_hcshader_missing_shader.cpp  @@ -11     - *   `Failed to find shader "<name>"` (CODE_NOSHADER) and no light. ...
                                           + *   `Failed to find shader "<name>"` (RIE_NOSHADER) and no light. ...
                                  @@ -55     -            if (r.text.find(expected) != std::string::npos && r.code == CODE_NOSHADER)
                                           +            if (r.text.find(expected) != std::string::npos && r.code == RIE_NOSHADER)
                                  @@ -57     -        REG_CHECK_MSG(noShaderCode, "%s: the error carries CODE_NOSHADER", light);
                                           +        REG_CHECK_MSG(noShaderCode, "%s: the error carries RIE_NOSHADER", light);
test_registry_frame_abort.cpp     @@ -75     -            if (r.code == CODE_BUG && r.text.find("could not create its thread") != std::string::npos)
                                           +            if (r.code == RIE_BUG && r.text.find("could not create its thread") != std::string::npos)
                                  @@ -77     -        REG_CHECK_MSG(bug, "the error carries CODE_BUG");
                                           +        REG_CHECK_MSG(bug, "the error carries RIE_BUG");
```

(The `frame_abort` header comment at :11 still reads "CODE_BUG"; it was outside the approved lines and is left.) `frame_abort` stays behind the red option, so it is not compiled in this tree.

| Check | Result |
|---|---|
| CP-a build (`030-p4b-build`) | exit 0 |
| T058 explicit set (`030-p4b-lights`) | **7 of 7 passed**: the four `Visual_*light*`, `Sampling_ThreadSweep_quadlight`, `Hcshader_MissingShader`, `Hcshader_NoReferences` |
| CP-b suite (`030-p4b-suite`, `-LE determinism-sweep -j4`) | **1103 run, 1103 passed, 0 failed**; Not Run: the two helper tests and `Registry_CoreNamesNoHider` (`DISABLED`), self-skip `LibShader_Format_rslo_lower_minor` |
| CP-d | manifest 915, fresh 921, **0 differing, 0 missing**, 6 not in manifest; `diffs` equal to `baseline-diffs-macos.txt` |
| CP-c (`030-p4b-arch`, `030-p4b-archreport`) | `ctest -L architecture` 32 of 32; report `ALLOWED`/`METRIC`/`RESULT` lines equal `archcheck-base-macos.txt` (`DB-011 x 164`, `TOTAL 604/89`, `RESULT PASS`) |

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

### Phase 3 (T042; Test gate for the Transport class)

With the amendment, `domain-rules.txt`, `domain-map.txt` and the two fixtures in place but before the `Class::Transport` edit of `archcheck`, `ctest -R ArchFixture_transport` (`030-p3-red`, after a reconfigure; `archcheck` and both fixture objects built): **0 of 2 passed**. `ArchFixture_transport_allowed`: `archcheck: .../fixtures/transport-allowed/domain-map.txt:8: unknown class 'Transport'`, exit 2, expected 0. `ArchFixture_transport_cycle`: the same message, exit 2, expected 1. Green after T043 (§1).

### Phase 4 (T050; Test gate for the retirement; 2026-11-10)

`Hcshader_MissingShader` moved out of the `OPENRENDER_030_RED_TESTS` block in `tests/unit/registry/CMakeLists.txt` and `Hcshader_NoReferences` removed from the `DISABLED` line in `tests/architecture/CMakeLists.txt` (the test sources and `check_hcshader_references.cmake` are untouched). Reconfigure (`030-p4-configure`), build of `test_hcshader_missing_shader` (`030-p4-tg-build`), `ctest -R Hcshader_` (`030-p4-tg-run`, `build-logs/030-p4-tg-run.log`) on the tree with `src/` still unedited: **0 of 2 passed**.
- `Hcshader_MissingShader` Failed by assertion: only `Failed to find shader "defaultsurface"` and `"defaultlight"` are reported (the empty search path); `CHECK failed: s.frames.count("Failed to find shader \"spherelight\"") == 1` and the same for `quadlight`, because the C++ substitute answers.
- `Hcshader_NoReferences` Failed: lists `src/libshader/shading/shaderRefs.h`, `shading.h`, `src/ri/CMakeLists.txt` (x5), `src/ri/hcshader.{h,cpp}`, `renderer.cpp`, `rendererContext.cpp` (x3), `rendererStatics.cpp` and `tests/architecture/domain-map.txt`; none of the exempt files and no registration line.


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

### Phase 3 light CP-L (T046; `linux_sync.sh push` 3814 files, `verify` 0 differences)

`030-linux-p3`: configure + build exit 0, 0 `warning:` lines under GCC 13 `-Werror`; `ctest -L architecture`: 31 of 31 passed (the two new script tests `Not Run (Disabled)`); `ctest -R ArchFixture_transport`: 2 of 2 passed (both new fixtures green on Linux).

### Phase 4 full CP-L (T059; 2026-11-10; `linux_sync.sh push` 3812 files, `verify` 0 differences; the two `src/ri/hcshader.*` files removed from the remote by the sync)

The host answered again after the maintainer's report (the first attempt, earlier the same day, had timed out). The remote `cpujob` was busy with spec 056's build for a while; the job `030-linux-p4` (`CPUJOB_TIMEOUT=7200`, `-j2`) queued behind it and ran `build-logs/run-p4.sh` in the remote tree (configure, build, suite, CP-d, architecture). Remote disk 8.5 GiB free.

| Check | Result |
|---|---|
| Build (Release, JIT on, GCC 13 `-Werror`) | exit 0, **0 `warning:` lines** |
| Suite (`-LE determinism-sweep -j2`, 688 s) | **1104 run, 1102 passed, 2 failed**: `TextureTile_RenderByteIdentical` (the recorded Linux starting failure, T016/#45) and `Hcshader_NoReferences` (F11, an environment limitation of the check, not a leftover reference). Not Run: the two helper tests and `Registry_CoreNamesNoHider` (`DISABLED`), self-skip `LibShader_Format_rslo_lower_minor`. 1104 against macOS's 1103: the one Linux-only test, as at the baseline. `Hcshader_MissingShader`, the four `Visual_*light*` and `Sampling_ThreadSweep_quadlight` **pass** |
| CP-d against the `-linux` baseline | manifest 915 images, fresh 921, **0 differing, 0 missing**, 6 not in manifest; `diffs` equal to `baseline-diffs-linux.txt` (empty diff) |
| `-L architecture` | 33 registered, 32 run: 31 passed and `Hcshader_NoReferences` failed (F11). Full report: `ALLOWED` identical to the baseline (`DB-011 x 164`), `METRIC TOTAL refs 604 distinct 89`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0` |
| `pull` + `verify` | 8 Linux-produced files, 0 differences (nothing new produced; the full-tree `verify` shows 4 differences only in `tasks.md` and `validation.md`, edited after the push) |

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
- **F9 (Phase 4, T058: `Hcshader_MissingShader` stays red after the retirement; a defect in the approved test, NOT edited, waits for the maintainer).** With `hcshader` deleted the renderer reports exactly what the test expects, `./hcshader_missing_spherelight.rib (7): Failed to find shader "spherelight"` (and the same for `quadlight`), once each; the first `REG_CHECK` (text, count 1) now passes. The second check fails: `test_hcshader_missing_shader.cpp:57` compares the report's `code` with `CODE_NOSHADER` (the `EErrorCode` enum, value 23), but the session recorder (`regtest::sessionHandler` in `registry_testing.h`) is an RI error handler and stores the **RI code** it is handed, `RIE_NOSHADER` = 45 (`error.cpp` maps `CODE_NOSHADER` to `RIE_NOSHADER`; the red run of the starting tree already printed `[code 45 severity 2]` for the missing `defaultsurface`). The comparison can never hold for a session report; the same report on a `CRecordingSink` would carry 23. Proposed one-line fix to the approved test, to be applied only on approval: `test_hcshader_missing_shader.cpp:56` `r.code == CODE_NOSHADER` -> `r.code == RIE_NOSHADER` (and the message at `:57` and the header comment "(CODE_NOSHADER)" -> "(RIE_NOSHADER)"); no source change, no threshold. **Same pattern, not yet reachable:** `test_registry_frame_abort.cpp:75` (Phase 9) compares a session report's code with `CODE_BUG` (value 10) where the handler stores `RIE_BUG`; check it when its gate opens (`test_registry_hider_table.cpp` and `test_registry_integrator_table.cpp` read a `CRecordingSink`, which stores `EErrorCode`, so their `CODE_*` comparisons are right). The result of T058 therefore reads "the four light visual tests and the sampling test pass, `Hcshader_NoReferences` green, `Hcshader_MissingShader` red for F9 only".
- **F10 (Phase 4, Linux).** The Linux host `juvenal@ubuntu-2404-desktop.v2-labs.io` was tried once at T059 (2026-11-10, `ssh -o ConnectTimeout=10`): `Operation timed out` on port 22. The full CP-L of Phase 4 is **pending** (see §3).
- **F9 resolved (2026-11-10).** The maintainer approved the two test edits (relayed by the coordinator): `test_hcshader_missing_shader.cpp` `CODE_NOSHADER` -> `RIE_NOSHADER` (:55, :57, header :11) and `test_registry_frame_abort.cpp` `CODE_BUG` -> `RIE_BUG` (:75, :77); diffs and the second macOS run in §1. T058 is 7 of 7.
- **F10 resolved.** The Linux host came back; the full Phase 4 CP-L ran (§3).
- **F11 (Phase 4, Linux; for the maintainer).** `Hcshader_NoReferences` fails in the Linux per-spec tree: `check_hcshader_references.cmake:37` lists the files with `git ls-files`, and the tree that `linux_sync.sh` builds has no `.git` (by design: `verify` fails on a remote `.git`), so the check ends with "cannot list the tracked files (git ls-files: 128 fatal: not a git repository)". It is a failure of the check's environment, not a reference: a `grep -rnE 'hcshader|CSphereLight|CQuadLight' src CMake tests doc docs examples` over the remote tree, minus the same exemptions and the script's own registrations, finds **no hit**. The same limitation applies to every later spec 030 script test that lists files with git (`Registry_CoreNamesNoHider` uses explicit paths, so it is not affected). Options for the maintainer: (a) accept it as a known Linux-tree limit and compare the Linux set to "T016 plus `Hcshader_NoReferences`"; (b) approve a fallback in the approved script (when `git ls-files` fails, `file(GLOB_RECURSE)` over the five scanned directories, same exemptions), which changes an approved test and is therefore not applied. T059 stays unticked until the maintainer chooses.
- **F11 resolved (2026-11-10)**; the added `cmake_policy(VERSION 3.19)` line was approved by the maintainer the same day, by the maintainer ruling "no test or build step may use git" and the approved script rewrite; see the section "F11 ruling and its fix" at the end of this file's §4.

### F11 ruling and its fix (2026-11-10; relayed by the coordinator)

**Ruling (maintainer, 2026-11-10):** no test or build step may use git, because a source tarball has no `.git` and may have no git installed. This is broader than the options offered in F11 and binds every later script and test of this spec. **Approved edits to approved tests:** (1) `tests/architecture/check_hcshader_references.cmake`: remove the `git ls-files` call completely (no fallback, no skip) and list the files with `file(GLOB_RECURSE ... LIST_DIRECTORIES false)` over `src CMake tests doc docs examples`, exemptions unchanged, build trees excluded, comments and error text fixed; the registration in `tests/architecture/CMakeLists.txt` needed no change and is untouched; (2) `tests/unit/registry/test_registry_frame_abort.cpp:11` header comment `CODE_BUG` -> `RIE_BUG`.

Diff of the script (`git diff -U0`, the pure removal of git and its replacement):

```
-# ... no tracked file under src/, ...                       +# ... no file under src/, ...
-# It never passes vacuously: no tracked file list, or an empty scan, is an error.
+# The files are listed with file(GLOB_RECURSE) over those six directories: ... no .git (and no git installed). A path with a
+# build-tree component (CMakeFiles, build*, _build, cmake-build*) is skipped ...; symbolic links are not followed.
+# It never passes vacuously: an empty or implausibly small scan is an error.
+cmake_policy(VERSION 3.19)                                   (see below)
-execute_process(COMMAND git -C "${SOURCE_DIR}" ls-files -- src CMake tests doc docs examples ...)
-if(NOT _rc EQUAL 0 OR "${_files}" STREQUAL "")  message(FATAL_ERROR "... cannot list the tracked files (git ls-files: ...)")
-string(REPLACE "\n" ";" _files "${_files}")
+set(_files "")
+foreach(_dir src CMake tests doc docs examples)
+    file(GLOB_RECURSE _found LIST_DIRECTORIES false RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/${_dir}/*")
+    list(APPEND _files ${_found})
+endforeach()
+if("${_files}" STREQUAL "")  message(FATAL_ERROR "check_hcshader_references: no file found under ...")
+set(_build_tree_component "(^|/)(CMakeFiles|build[^/]*|_build|cmake-build[^/]*)/")
+    if(_f MATCHES "${_build_tree_component}")  continue()  endif()      (in the scan loop, before the exemption test)
-message(STATUS "... ${_scanned} tracked files scanned, no reference")  +message(STATUS "... ${_scanned} files scanned, no reference")
```

and of `test_registry_frame_abort.cpp`: `:11` `(CODE_BUG, "could not create ...` -> `(RIE_BUG, ...` (with :75 and :77 from the earlier approval).

**One addition not in the approved list: `cmake_policy(VERSION 3.19)` (approved by the maintainer 2026-11-10, relayed by the coordinator: kept).** Once the script reached its scan on Linux (CMake 3.28), `-P` script mode left CMP0057 unset, so `if(_f IN_LIST _exempt_files)` failed with "if given arguments"; macOS's newer CMake defaults the policy to NEW, which hid it (the script used to fail earlier, at `git`, on Linux). One line, no behaviour change on a CMake that already reads `IN_LIST`; 3.19 is the project floor. No other script of the architecture or link tests uses `IN_LIST`.

**Generated or untracked files in the six scanned trees (answer to the coordinator's question).** On a normal macOS checkout after a build there are two kinds, measured with `find` against `git ls-files`: `src/preview/orender-wire-macos/CRibPreview/include/ribpreview_api.h` (generated by the build, gitignored: a copy of a tracked header, scanned, no reference) and `src/python/__pycache__/prman.cpython-315.pyc` (not a scanned extension). The script's extension filter drops binaries and caches; its build-tree rule skips any path with a `CMakeFiles`, `build*`, `_build` or `cmake-build*` directory component, so a build directory configured inside one of the six trees is not scanned. The project's own build trees (`build`, `build-base`, `build-debug`, `build-logs`) sit at the root, outside the six, and no tracked path under them contains such a component (checked). The glob gives 2009 files on the real tree; `git ls-files` gave 2008 after the same filters, the one extra being the generated `ribpreview_api.h`.

**Red then green, macOS** (`cmake -P` through `cpujob`, scratch copy of the six directories under the scratchpad, prefix `030-`, no `.git`):

| Case | Result |
|---|---|
| Real tree | `Hcshader_NoReferences: 2009 files scanned, no reference`, exit 0 |
| Scratch copy, clean | the same, exit 0 |
| Scratch copy with a planted `src/ri/030_planted.cpp` holding `CSphereLight`/`hcshader` | **fails**: `src/ri/030_planted.cpp: // uses CSphereLight from hcshader.h`; a planted `doc/030_planted.html` holding `CQuadLight` also fails, exit 1 (final script) |
| Scratch copy, planted file removed | green again, exit 0 |
| Same text inside `src/build-x/CMakeFiles/gen.cpp` | skipped (build-tree rule), passes |
| Clean scratch copy with `PATH=/nonexistent` (no git, no other tool) | passes, exit 0 |
| `ctest -L architecture` on the real tree (`030-p4c-arch`, `030-p4d-arch`) | 32 of 32 passed, `Hcshader_NoReferences` Passed; T058 set 7 of 7 |

**Linux** (`linux_sync.sh push` 3812 files, `verify` 0 differences; build exit 0, 0 warnings; remote `cpujob`, `-j2`): first run `-L architecture` failed only on the `IN_LIST` policy above (31 of 32); after `cmake_policy(VERSION 3.19)`: **`ctest -L architecture` 32 of 32 passed** (33 registered, `Registry_CoreNamesNoHider` `Not Run (Disabled)`), and the T058 set **7 of 7 passed** (`Visual_quadlight`, `-slo`, `Visual_spherelight`, `-slo`, `Sampling_ThreadSweep_quadlight`, `Hcshader_MissingShader`, `Hcshader_NoReferences`). The earlier Linux suite and CP-d (§3) are unchanged by this edit (no `src/` or image-affecting change). The Linux pass/fail set therefore equals T016's.

## §5 Final summary against SC-001 to SC-011

(at Phase 10)
