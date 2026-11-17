# Validation record: spec 033 (shading system backends)

Written during implement. Phase 1 fills sections 0 to 4; section 5 is filled in Phase 9. Evidence files live beside it in
`specs/033-shading-system-backends/tools/`. Job logs are in `build-logs/` (gitignored). Times are America/Sao_Paulo, 2026-11-15.

## 0. Starting-tree measurements and orientation (Phase 1)

### 0.1 Orientation (T003)

- Worktree `/Volumes/Projects/Development/CLI/openrender-worktrees/033-shading-system-backends`, branch `033-shading-system-backends`,
  HEAD `50d74ec3` (base master `278b0394`). At the start only `specs/033-shading-system-backends/` was staged. `src/` is untouched.
- `.specify/feature.json` names `specs/033-shading-system-backends`. `build`, `build-nojit`, `build-logs`, `build-pkg` are gitignored.
- CLAUDE.md "Spec-kit agent rules": both copies (this worktree and the main checkout) carry the section with identical text
  (`diff` of the section: no difference); the **worktree copy** was followed.
- Disk: 16 GiB free on `/Volumes/Projects` at the start and at the end. Remote `/home` 7.5 GB free at the start, 7.3 GB after the build.
- Other specs (040, 048, 057) queue on the same `cpujob` lock; macOS jobs waited for it, sometimes 20 minutes.

### 0.2 macOS baseline (T007 to T012)

| Item | Result |
|---|---|
| T007 configure and build `build` (JIT on, `-Werror`) | both `rc=0`; the tree was current (build 7 s) |
| T008 suite 1 (`-LE determinism-sweep -j4`) | **1363 tests, 100% passed**; 1 self-skip (`LibShader_Format_rslo_lower_minor`); 368 s. `baseline-images-macos.sha256` 1194 images; `baseline-diffs-macos.txt` 400 rows |
| T009 suite 2 | 1363/1363 (356 s); **0 images differ between the runs, the `MaxBlockAvgDiff` listings are identical**; `baseline-image-noise-macos.txt` is header only |
| T010 `archcheck` | `RESULT PASS violations=0 stale=0 badid=0`; `ALLOWED DB-004 x 3 DB-011 x 144`; `CONSUMER` **71** lines (sloinfo 3: `libshader/runtime/rslo.h`, `shading/llvmJit.h`, `shading/sloMetadata.h`; oshader 3: `compiler/llvmEmitter.h`, `opcodes.h`, `rslo.h`); `METRIC` TOTAL 491/71, Shading 0/0, Hiders 489/69, Geometry 2/2 (all equal the plan's R0.1) |
| T011 determinism sweep (`-j2`) | **582/582 passed** twice (787 s, 691 s); `baseline-sweep-images-macos.sha256` 582 images, **0 differ between the two runs** (`baseline-sweep-noise-macos.txt` header only) |
| T012 JIT-off (`build-nojit`, 137 `.rslo`, no `.slo`) | build `rc=0`; 1061 tests, 1057 passed. The 4 non-passes: `Visual_teapot-motion-raytrace` and `ShadingParity_JitPLEngaged_trace_samples_cone_rslo` (**Timeout**; both pass when re-run alone: `build-logs/033-base-nojit-rerun.log`, the machine was loaded by other specs' jobs), `Registry_IntegratorImageIdentity` (the photon rows, GitHub #163, a known intermittent), and **`Shaders_BytesUnchanged`** (see section 4, finding F-4: its `oshader --help` reference lists `--jit`, which a JIT-less `oshader` does not print; a property of the starting tree, not of this spec) |

### 0.3 Linux baseline (T020 to T024; ubuntu-2404-desktop, GCC 13, `-Werror`, per-spec tree `openrender-033`)

| Item | Result |
|---|---|
| T020 sync and configure | `linux_sync.sh push` 4339 files, `verify` "4339 files, 0 differences", no `.git` on the remote; `cmake -B build -S .` `rc=0` |
| T021 build `-j2` | `rc=0` in 8 minutes, no GCC-only error; `ctest -L architecture`: `RESULT PASS`, `CONSUMER` 71, METRIC TOTAL 491/71 (`archcheck-base-linux.txt`) |
| T022 suite 1 and 2 (`-LE determinism-sweep -j2`) | **1364 tests, 1362 passed** in both runs (701 s, 684 s). Failing set: **`TextureTile_RenderByteIdentical`** (GitHub #45, held until spec 042; the `texture-png/exr/rgbe-reyes` and `texture-exr-raytrace` outputs differ from the macOS-rendered references at byte 5) and **`Registry_IntegratorImageIdentity`** (photon rows, GitHub #163). 1 self-skip. 1194 images in `baseline-images-linux.sha256`; **0 differ between the runs**; the `MaxBlockAvgDiff` listings are identical |
| T023 sweep | **582/582 passed** twice (1389 s, 1348 s); `baseline-sweep-images-linux.sha256` 582 images, **0 differ between runs** |
| T024 pull | `linux_sync.sh pull` and `verify`: "8 files, 0 differences" (the Linux baselines, the Linux `ShaderFormat_Precedence` reference); every file the task lists exists locally |

The Linux failing set (two tests) is the starting tree's, not this spec's. It is the same two the earlier Linux baselines recorded (spec 031: #45 and #163).

### 0.4 Characterisations (T013 to T017)

All five are green on the unchanged tree on both platforms, run **at least three times each** with an unchanged result (the first run of each
reference records it, the later runs compare; `check_recorded.cmake`, `run_precedence.cmake` and `test_sloinfo_output137.sh` never replace an existing
reference). `build-logs/033-p1-char*.log`, `033-p1-nojit-char*.log`, `033-l1-char*.log`.

| Test | Label | What the record holds | Result |
|---|---|---|---|
| `ShaderFormat_Precedence` | `shaderformat` | `tests/shaderformat/reference/macos/precedence.txt` (62 cases), `.../linux/precedence.txt` (62), `.../macos/precedence-nojit.txt` (26, the `build-nojit` tree) | passes in `build`, `build-nojit` and Linux. **The macOS and Linux records differ in two lines only** (the JIT loader's symbol text `_fmtd` on macOS, `fmtd` on Linux), so Linux keeps its own copy |
| `BindSpaces_Characterise` | `shading_contract` | `tests/shading_contract/reference/bind_spaces.txt`: 30 parameter defaults of a surface and 30 of a light (point, vector, normal in 10 spaces) for `.rslo` and for `.slo`, as the raw 32-bit patterns beside the decimals | passes on macOS and, **against the same file**, on Linux (bit for bit). **The `.rslo` and the `.slo` sections are identical** (0 of 60 entries differ) |
| `ShadingContract_MessageRequests` | `shading_contract` | `tests/shading_contract/reference/message_requests.txt`: six sections (`rslo` and `slo` times `matrices`, `attributes`, `messages`) | passes on macOS and, against the same file, on Linux. **The `.rslo` and `.slo` request sequences are identical** within P-Q3's scope (the one-light `illuminance` loop). `shadername()`: "no requests" under both backends (what `ShadingContract_MessageRequestsBoth` is red on in Phase 2) |
| `Sloinfo_Output137` | `libshader` | `tests/shaders/references/sloinfo137/{rslo,slo}.txt`: exit code, stdout and stderr of `sloinfo` for 137 shaders in each format (all exit 0, empty stderr) | passes in `build` and `build-nojit` (the `.rslo` half) and on Linux against the macOS files. `SloinfoGoldenOutput` unchanged and green |

What `ShaderFormat_Precedence` records on the unchanged tree (macOS, JIT on): `.rslo` renders red, `.slo` green.

| attribute | option | both files | `.rslo` only | `.slo` only |
|---|---|---|---|---|
| `slo` | any | slo | rslo | slo |
| `rslo` | any | rslo | rslo | slo |
| unset | `slo` | slo | rslo | slo |
| unset | `rslo` or unset | rslo | rslo | slo |
| invalid (`bogus`) | any | slo (the attribute turns an invalid value into `"slo"` with a warning, `CAttributes::setShaderFormat`) | rslo | slo |

With neither file the render uses the default surface and reports `Failed to find shader "fmtp"` (exit 255). Further cases: a dotted name is looked
up as written whatever the preference; a file named `.slo` that is not bitcode falls through silently to the `.rslo` (none: "Failed to find"); an `.slo`
that calls a shadeop operand whose module is not in the search path warns "Failed to locate DSO function probe_add" and, with `rslo` preferred, the
`.rslo` is tried first and fails the same way; **the cache case records the first format winning for the whole scene** (a name asked for `slo` and then
`rslo` renders slo in both places, and the reverse renders rslo in both: GitHub #167). In the JIT-less tree every `slo` request warns once per
uncached request ("shaderformat "slo" requested, but this build has no LLVM JIT support") and renders through the interpreter.

What `BindSpaces_Characterise` records: `"current"` and `"shader"` give the same value (the instance's transform is the current one, as research R6 said),
`"object"`, `"light"` and `"camera"`/`"NDC"`/`"raster"`/`"screen"` (no frame is begun at bind time) leave the value unchanged, `"world"` and the
declared system `cmuser` are transformed. Both backends agree to the bit. A matrix default in a named space is **not** recorded (finding F-1).

### 0.5 Timings (T018, `tools/timing-base-macos.txt`, `cpujob 033-timing`, five runs each, the first discarded)

| Figure | Median | Min | Max | Spread (max - min) |
|---|---|---|---|---|
| `sloinfo`, 100 loads x all 274 objects (`tests/benchmark_loading.sh`), wall time of one pass | 131.73 s | 124.43 s | 139.88 s | 15.46 s |
| in-process `.rslo` loads of 137 objects (`bench_shader_load`) | 9.480 ms | 9.308 ms | 9.819 ms | 0.511 ms |
| in-process `.slo` loads of 137 objects | 615.369 ms | 600.064 ms | 632.000 ms | 31.936 ms |

Phase 9's rule: after <= before + max(5% of before, 2 x the spread above). The wall figure's spread is 11.7% of its median, so the tolerance for it
is 23%; X. The machine is shared with other specs, so a re-run at Phase 9 should
quote the same conditions.

### 0.6 The plan's measurements re-checked (T019)

| Claim | Measured | Deviation |
|---|---|---|
| `kRendererServicesVersion` 2.3.0 (`rendererServices.h`) | `{2, 3, 0}` at line 131 | none |
| `rendererContext.cpp:298-350` (`getShader`), `rendererFiles.cpp:372-439`, `execute.cpp:502` | `getShader` starts at 298; the JIT dispatch `if (cInstance->jitEntry != nullptr)` is at `execute.cpp:502` | none that matters (the `src/libshader/shading` copy is the only `execute.cpp`) |
| `op_lightsource_f` at `rslOps.cpp:921-962`, reading `getParameterShape()` | the function starts at 921 and the call is at 938 | none |
| `shaderName(type)` reads attributes at `shading.cpp:2907-2939` | the `currentAttributes` reads are at 2909 to 2932 (12 lines) | none |
| the eight `op_*_param` entries reach the services | `rslOps.cpp:2219-2272` | none |
| `lightInstanceOf` at `shaderRefs.h:43` | line 43 | none |
| `init.cpp` 426 lines, 19 `findCoordinateSystem` and 1 `retrieveVariable` | 426 lines; the **preprocessed** body (`c++ ... -E -P` with `ribRender_obj`'s flags) holds **20** `CRenderer::findCoordinateSystem` (the 19 macro sites plus the JIT's `initCoordinateResolver`) and 1 `CRenderer::retrieveVariable`. Phase 3's "20 hidden references -> 0" counts these | none; 19 + 1 = 20 |
| scans of contracts/checks-and-tests.md | `tools/scan-base.txt`: 31 identifier lines + 4 `OPENRENDER_HAVE_LLVM` lines = **35** (`init.cpp` 19 + 2, `rendererFiles.cpp` 6 + 2, `rendererContext.cpp` 2, `rendererContext.h` 1, `rendererFileSearch.cpp` 2, `src/ri/CMakeLists.txt` 1); `Shading_JitMessagesThroughServices`: 2 lines in `rslOps.cpp` (`getParameter` 924, `getParameterShape` 938) and 12 `currentAttributes` lines in `shaderName` bodies; `Sloinfo_MetadataApiOnly`: 3 includes (`rslo.h`, `llvmJit.h`, `sloMetadata.h`) and 11 parser-name lines | none |

### 0.7 Test-edit inventory (T006, `tools/test-edit-inventory.txt`)

Measured with `grep` over `tests/` and `src/`. The approved tests on plan.md's and contracts/checks-and-tests.md's lists all appear. Approved test files
that name the identifiers and are **not on the "checked unchanged" list** (each only names `CProgrammableShaderInstance` or `CRenderer::getShader`, which P-Q6
keeps, so none is expected to need an edit; listed so the maintainer can see the full set):

- `tests/shading_contract/`: `case_real_answers.cpp`, `case_no_services.cpp`, `case_lightsource_no_loop.cpp`, `case_matrices.cpp`, `case_hider_tracer.cpp`,
  `case_null_object_messages.cpp`, `case_trace_requests.cpp`, `case_gather_requests.cpp`, `case_type_mismatch_message.cpp`, `case_type_mismatch_real.cpp`,
  `case_type_mismatch_real_message.cpp`, `case_type_mismatch_attribute.cpp`, `case_canned_bundles.cpp`, `case_canned_trace.cpp`, `case_link.cpp`,
  `case_messages.cpp`, `case_lightsource_no_loop_real.cpp`, `case_no_tracer.cpp`, `case_null_object.cpp`, `case_point_source.cpp`, `case_attributes.cpp`
  (all name `CProgrammableShaderInstance` or, in `case_link.cpp`, the library)
- `tests/shading_parity/`: `CMakeLists.txt`, `test_texture_substitute.cpp`, `test_texture_substitute_3d.cpp` (`CProgrammableShaderInstance`),
  `test_shader_parameter_equivalence.cpp` (`CRenderer::getShader`)
- `tests/unit/state/CMakeLists.txt` (names `shader.h` in a comment or list)

For the library rename the names `libshader_shading`/`shader_shading` appear in 26 to 28 test files, but **only as comments or through the link target alias** except the
three the plan names: `tests/link/check_scope.cmake:86` (`CMakeFiles/libshader_shading.dir/flags.make`), `tests/shading_contract/check_link_line.cmake:56`
(`"shader_shading"`), `tests/plugins/checks/install_consumer/CMakeLists.txt:16`. `libshader_runtime`/`shader_runtime` appear in `tests/unit/common/CMakeLists.txt`
and the same install consumer, as the plan says. No other pinned library file name was found.

## 1. Checkpoint records

Phase 1: the baselines are the checkpoint (nothing in `src/` changed). `git diff --stat HEAD -- src` is empty. The edits to existing files are the three
registrations T001 announced: `tests/CMakeLists.txt` (+2 lines), `tests/shaders/CMakeLists.txt` (+27), `tests/shading_contract/CMakeLists.txt` (+69).
`ctest -L architecture` on the tree with the new tests: macOS 36/36 passed (`033-p1-arch2.log`), Linux 36/36 passed. The allowlist and `metrics-baseline.txt` are untouched.

### Phase 2 (T045; the option `OPENRENDER_033_RED_TESTS` OFF)

| Check | macOS | Linux (GCC 13, `-Werror`) |
|---|---|---|
| CP-a build | `rc=0` | `rc=0` |
| CP-b suite (`-LE determinism-sweep`) | **1367 tests, 100% passed** (1363 + the four Phase 1 characterisations); the 16 new tests show "Not Run (Disabled)" | **1368 tests, 1366 passed**: the baseline's two failures only (`TextureTile_RenderByteIdentical` #45, `Registry_IntegratorImageIdentity` #163); 701 s |
| CP-d images | 0 differing, 0 missing (64 fresh images of the new characterisations, information only); `MaxBlockAvgDiff` rows identical to `baseline-diffs-macos.txt` (empty diff) | 0 differing, 0 missing; `baseline-diffs-linux.txt` rows identical |
| CP-c `ctest -L architecture` | 36/36; `archcheck`: `RESULT PASS violations=0 stale=0 badid=0`, `ALLOWED DB-004 x 3 DB-011 x 144`, `CONSUMER` 71, `METRIC` TOTAL 491/71 and Shading 0/0 (unchanged); `git diff --stat -- tests/architecture/allowlist.txt` empty | 36/36; the same `archcheck` figures |
| CP-m contract tests (`^shading_contract$|^shading_parity$|^libshader$|^shaderformat$|^shading_system$`) | 240/240 | 240/240 |
| replays and baselines | `git diff --stat HEAD -- tests/shading_contract/replay tests/unit/motion/replay specs/033-shading-system-backends/tools/baseline*` empty; `git diff HEAD -- src` empty | same tree |

CP-v does not apply before Phase 3. The macOS CP-b run was 194 s for the contract set and 3.5 minutes for the suite; logs `build-logs/033-p2-*.log`, `033-l2-*.log`.

### Phase 3 (T056 to T059; the bind-time init moved into Shading, services 2.4.0 begun)

| Check | macOS | Linux (GCC 13, `-Werror`) |
|---|---|---|
| CP-a build | `build` and `build-nojit`: `rc=0`, no warning from the changed files | `rc=0` |
| T056 characterisations | `BindSpaces_Characterise` **identical** to the Phase 1 record (both backends, bit for bit); `ShaderFormat_Precedence` identical in `build` and `build-nojit`; `ShadingContract_MessageRequests` and `Sloinfo_Output137` identical; `ShadingSystem_BindUnknownSpace` green on both backends | in the suite below, identical |
| CP-b suite | **1368/1368** (1367 + `ShadingSystem_BindUnknownSpace`), 349 s; 15 new tests still "Not Run (Disabled)" | **1369 tests, 1367 passed**: the baseline's two failures only (#45, #163) |
| CP-d images | 0 differing, 0 missing; `MaxBlockAvgDiff` rows identical (empty diff) | 0 differing, 0 missing; rows identical |
| CP-c architecture | 36/36; `archcheck` PASS, `ALLOWED DB-004 x 3 DB-011 x 144`, `CONSUMER` 71, `METRIC` TOTAL 491/71, **Shading 0/0**; allowlist diff empty | 36/36; the same figures |
| CP-m contract tests | 241/241 | 241/241 |
| CP-v | no `static_assert` line differs in `rendererServicesImpl.h`, the contract doubles or `recording_services.h` since `278b0394`; `kRendererServicesVersion` reads `{2, 4, 0}` | same tree |
| SC-007 (T057) | `c++ <libshader_shading flags> -E -P` over `rsloInit.cpp`: `CRenderer::` **0**, `CRendererContext` 0; `sloInit.cpp`: 0 and 0 (`bindCoordinateSystem` 20 times in `rsloInit.cpp`: the 19 macro sites and the definition of the macro) | |

The 20 references of the starting tree (19 `CRenderer::findCoordinateSystem` and 1 `CRenderer::retrieveVariable`, `init.cpp`) are now 0 in the core and in Shading: the init asks the services.
Replays, baselines, the allowlist and `src/` outside the files of the approved list are unchanged.

### Phase 4 (T075 to T077; the split, the backends and the dispatch)

| Check | macOS | Linux (GCC 13, `-Werror`) |
|---|---|---|
| CP-a build | `build` and `build-nojit`: `rc=0`; **every approved test compiled unchanged** (the full build builds them all); `git diff --stat HEAD -- tests` **empty** | `rc=0` |
| Characterisations | `BindSpaces_Characterise`, `ShaderFormat_Precedence` (also in `build-nojit`), `MessageRequests`, `Sloinfo_Output137`, `ShadingSystem_BindUnknownSpace` identical / green | in the suite, identical |
| CP-b suite | **1367 of 1368 passed** on the first run; the one failure is `GiKeys_PhotonsThreads` (see below: an intermittent of the starting tree), which passes when re-run alone (2 of 3 re-runs) | 1369 tests, 1367 passed: the baseline's two failures (#45, #163) |
| CP-d images | 0 differing, 0 missing (the `-slo` and `-rslo` pairs included); `MaxBlockAvgDiff` rows identical (empty diff) | 0 differing, 0 missing; rows identical |
| CP-c architecture | 36/36; `archcheck` PASS, `CONSUMER` 71, TOTAL 491/71, Shading 0/0; allowlist diff empty | 36/36; same |
| CP-m contract tests | 241/241 (the spec 026 trace replay, the spec 028 motion replay, `shading_parity` included) | 241/241 |
| CP-v | no `static_assert` line of the implementers differs since `278b0394`; version `{2, 4, 0}` | same tree |
| `Visual_*-slo` timing ratio tests | all pass, `MAX_RATIO` unchanged | all pass |

**Intermittent `GiKeys_PhotonsThreads` (finding F-6, known: GitHub #164, the multithreaded photon crash, open on master and fixed on spec 040's branch; nothing to file; a red one is re-run alone once and treated as #164 until 040 merges).** The test (label `gi_split_threads`, `test_gi_keys photons-dump 1` then `photons-dump 4`) failed once in the macOS suite with `FAIL: a child render failed` after 0.04 s. Re-run alone: pass, pass, fail. A loop of 60 runs on macOS (this tree): 2 failures. The **same loop of 100 on Linux, run on the Phase 3 binaries (before any Phase 4 change)**: 3 failures with the same message. It is therefore an intermittent of the starting tree's four-thread photon dump, not a result of the split. It passed in all four baseline runs (two per platform) by luck of the odds (about 3% a run). It belongs with the known photon race (#163's family). For filing; nothing was changed for it.

**The split.** `CShader` keeps `name`, `type`, `parameters`, `usedParameters`, `flags`, `data` and gains `backend` and an owned `payload`; the tables, the strings, the frame layout and the JIT entries moved into `CRsloPayload` / `CSloPayload` (`shaderPayload.h`); `shader.h` no longer includes `llvmJit.h`.
**The dispatch invariant**, checked by grep of every assignment in `src/`: `jitEntry` and `jitInitEntry` are assigned only in `sloLoader.cpp` (`parseSloShader`); nothing else writes them, so a non-null `jitEntry` is exactly "loaded by `parseSloShader` and compiled", and a `CSloPayload` with a null `jitEntry` is "loaded by `parseSloShader`, JIT compile failed", which the JIT backend answers with the unchanged `CODE_SYSTEM` "has no bytecode and no JIT entry" warning.

### Phase 5 (T090 to T095; the ShadingSystem facade, the cache, the format order, the core's resolver and bind host)

| Check | macOS | Linux (GCC 13, `-Werror`) |
|---|---|---|
| CP-a build | `build` and `build-nojit`: `rc=0` | `rc=0` (4 minutes at `-j2`, no GCC-only error) |
| The new tests | `ShadingSystem_ThirdBackend`, `_Precedence`, `_Cache`, `Shading_CoreNoBackendNames` and its fixture run green; `ShadingSystem_NoJitFallback` green in `build-nojit` | in the suite and the contract set, green |
| Characterisations (T091) | `ShaderFormat_Precedence` identical in `build` and in `build-nojit` (each against its own reference); `ShadingContract_MessageRequests` and `BindSpaces_Characterise` identical | in the contract set |
| CP-b suite (`-LE determinism-sweep`) | **1373 of 1373 passed** (1383 listed, the rest disabled or skipped) | 1374 tests, 1372 passed: the baseline's two failures only (`TextureTile_RenderByteIdentical` #45, `Registry_IntegratorImageIdentity` #163); 12 minutes |
| CP-d images | 0 differing, 0 missing (64 fresh images of the characterisations, information only); `MaxBlockAvgDiff` rows identical to `baseline-diffs-macos.txt` (empty diff) | 0 differing, 0 missing; rows identical to `baseline-diffs-linux.txt` |
| CP-c `ctest -L architecture` | 38/38; `archcheck`: `RESULT PASS violations=0 stale=0 badid=0`, `ALLOWED DB-004 x 3 DB-011 x 144`, `CONSUMER` 71, `METRIC` TOTAL 491/71 and Shading 0/0; `git diff --stat HEAD -- tests/architecture/allowlist.txt` empty | 38/38; the same `archcheck` figures |
| CP-m contract tests | 244/244 | 244/244 |
| CP-v | no `static_assert` line differs since `278b0394`; `rendererServices.h` and `serviceTypes.h` unchanged this phase; version `{2, 4, 0}` | same tree |
| SC-002 (the scan) | `Shading_CoreNoBackendNames`: 0 violations (the starting tree: 35 lines) | same |
| SC-003 (T092) | `git diff --stat HEAD -- src/display src/shadeop src/procedures src/common` empty; `src/ri` holds only the migration files; `tests/` holds only the two CMake gates. `test_shading_system_third_backend` links `libshader_shading`, `libRIBCommon` and the State-only static library (no `ribRender`), with `-Isrc -Isrc/libshader/shading -Isrc/ri/*` | |

`CRenderer::shadingSystem` replaces `globalFiles` (used for shaders only); `CRenderer::getShader(name, path[, preferredType])` forwards to `ShadingSystem::find()`; `CRendererContext::getShader` keeps its signature, answers `"null"` before the facade (the leak-fix spec's ground), builds a `ShaderRequest` and calls `bind()`. `OPENRENDER_HAVE_LLVM` appears 0 times under `src/ri`. The cache is keyed by the description's own `name`, consulted before the format is resolved (#167), under its own `std::mutex`, never `shaderMutex`.
`git diff 278b0394 -- src/ri/render/rendererContext.cpp`: three hunks (the include, `getShader`, the new `registerLight`); `RiLightSourceV`, `RiAreaLightSourceV`, `RiIlluminate`, `extractLightHandle`, `takeInstanceKey` and `pendingInstanceKey` untouched.

### Phase 6 (T098 to T103; the group builder and the group run helpers)

| Check | macOS | Linux (GCC 13, `-Werror`) |
|---|---|---|
| CP-a build | `rc=0` | `rc=0` |
| `ShadingSystem_GroupOneLayer` | green (bit-identical `Ci`/`Oi` by group and by instance; `connect()` and a second `shader()` refused with `CODE_INCAPABLE`, nothing loaded or keyed; `end()` returns the instance with one reference) | green |
| `ShadingSystem_BindOrder` | green after the wrong-type shader and the harness fix (below) | green |
| CP-b suite (`-LE determinism-sweep`) | 1375 tests, 1374 passed; the one failure was `ShadingSystem_BindOrder` (then fixed: see below) | 1376 tests, 1373 passed: the baseline's two (#45, #163) and `ShadingSystem_BindOrder` (then fixed) |
| CP-d images | 0 differing, 0 missing (64 fresh images, information only); `MaxBlockAvgDiff` rows identical (empty diff) | 0 differing, 0 missing; rows identical |
| CP-c architecture | 38/38; `archcheck` PASS, `ALLOWED DB-004 x 3 DB-011 x 144`, `CONSUMER` 71, TOTAL 491/71, Shading 0/0; allowlist diff empty | 38/38; same |
| CP-m contract tests | 246 tests, 245 passed (the same one failure) | same |
| CP-v | `rendererServices.h` and `serviceTypes.h` unchanged; no `static_assert` edited | same tree |

**`ShadingSystem_BindOrder` (an approved test, not edited beyond the maintainer's choice).** Its light block expects `load` first, but the wrong-type block of the same test had already loaded and cached `tstlight` (find or load, then the type check: the contract, and what `CRenderer::getShader` did before). The maintainer chose to give the wrong-type block its own light shader (`tstwrongtype`, a light requested as `SL_SURFACE`; the light block unchanged). That exposed a **latent use-after-free in the harness**: `CHarness` made its `CXform` with 0 references and "left it to process exit", but this test detaches its instances, so the surface block's `detach()` freed the transform and the light block's instance attached to freed memory (a Bus error once the heap layout changed). The maintainer's fix is in `tests/shading_contract/contract_harness.h`: the constructor takes one reference on its transform (`xform->attach()`, held to process exit like the rest). Only `tests/shading_contract` and `tests/shading_system` include the harness; `-L 'shading_contract|shading_system'` on macOS: 81 run, all passed (`BindOrder` included, 0.53 s).

### Phase 7 (T106 to T112; services 2.4.0 complete: `destBytes` and `getShaderName`)

| Check | macOS | Linux (GCC 13, `-Werror`) |
|---|---|---|
| CP-a build | `rc=0` | `rc=0` |
| The three tests | `ShadingContract_MessageRequestsBoth`, `ShadingContract_ShadernameNoServices`, `Shading_JitMessagesThroughServices` (and its fixture run) green; `ShadingContract_LightsourceOversizedJit` unchanged and green; `ShadingContract_MessageRequests` and `BindSpaces_Characterise` still match their Phase 1 records | green |
| CP-b suite (`-LE determinism-sweep`) | **1379 of 1379 passed** | 1380 tests, 1378 passed: the baseline's two (#45, #163) |
| CP-d images | 0 differing, 0 missing (65 fresh images, information only); `MaxBlockAvgDiff` rows identical (empty diff) | 0 differing, 0 missing; rows identical |
| CP-c architecture | 40/40; `archcheck` PASS, `ALLOWED DB-004 x 3 DB-011 x 144`, `CONSUMER` 71, TOTAL 491/71, Shading 0/0; allowlist diff empty | 40/40; same |
| CP-m contract tests | 248/248 | 248/248 |
| CP-v | `kRendererServicesVersion{2, 4, 0}`; no `static_assert` line differs; the replays and baselines untouched | same tree |

Red evidence (T106, recorded before the source edits): `ShadingContract_MessageRequestsBoth` failed to build (`no type named 'ShaderNameAnswer'`, `'ShaderNameRequest'`, `no member named 'destBytes' in MessageRequest`); `ShadingContract_ShadernameNoServices` ran and failed 5 assertions per backend (the real names came back, no warning: `sink.countText("renderer services do not provide shadername()") == 1`); `Shading_JitMessagesThroughServices` reported 14 violations (`rslOps.cpp:924 getParameter`, `:938 getParameterShape`, 12 `currentAttributes` lines in the `shaderName` bodies, `shading.cpp:2911-2934`).

What 2.4.0 now holds (the one history entry, written in Phase 3, already named everything and is unchanged): `MatrixRequest::phase`, `MessageRequest::destBytes`, `ShaderNameRequest`/`ShaderNameAnswer` and `getShaderName` (default Unavailable), the `execute()`/`interpret()` change and the internal types. The renderer answers `destBytes` before writing (TypeMismatch when the stored value needs more bytes, any source) and `getShaderName` through the instance selection `getMessage` shares (`selectMessageInstance`). `op_lightsource_f` asks one `getMessage` with 64 bytes and warns once on TypeMismatch (the same text); `shaderName(type)` asks `getShaderName` and returns `""` with one "renderer services do not provide shadername()" warning when the services do not answer.
**One departure from the task text.** The JIT's `jitShaderNameS` looped over the batch's points and would have asked once per point (9 requests for 3 points); the interpreter evaluates it once (uniform). To make both backends ask the same (the test counts exactly three), `jitShaderNameS` asks again only when the type string differs from the previous point's. The result is the same string at every point; no other JIT change.
Approved-test edit: `tests/shading_contract/contract_forwarder.h` forwards `getShaderName` to the inner services without recording it (so the Phase 1 record of `cm_shadername`, "no requests", still holds).

### Phase 8 (T116 to T135; the metadata API, `libRSLInfo`, and the library renames)

macOS (`build` and `build-nojit`; Linux is recorded in the table's right column when its run is done):

| Check | macOS | Linux (GCC 13, `-Werror`) |
|---|---|---|
| CP-a build | `build` and `build-nojit` rc=0 | `rc=0` (the first attempt failed on `ShaderMetadata.h`, see F-9; the re-run is the record) |
| Part A: `SloinfoGoldenOutput`, `Sloinfo_Output137` | green; `Sloinfo_Output137` identical to the Phase 1 record (137 shaders x 2 formats, byte for byte) | green (in the suite) |
| Part A: `ShaderMetadata_Shipped`, `Sloinfo_MetadataApiOnly` (+ fixture) | green | green |
| `sloinfo` links | `otool -L`: `libRSLInfo.1.dylib`, `libRIBCommon.1.dylib`, system libs; **not** `libRSLRuntime`; `sloinfo` is 34,648 bytes (was 6,441,784 with LLVM inside); `libRSLInfo.1.0.0.dylib` is 6,482,672 bytes | `ldd`: `libRSLInfo.so.1`, `libRIBCommon.so.1`; not `libRSLRuntime`; `sloinfo` 21,392 bytes |
| `sloinfo` start-up (R12 measurement 1, 100 loads x 274 objects, 5 passes, first discarded) | median **136.243 s**, min 130.384, max 147.500, spread 17.117 (Phase 1: 131.731, spread 15.459): +3.4%, inside the pass rule (before + max(5%, 2 x spread) = 162.6 s) | |
| in-process loads (R12 measurement 2) | `.rslo` median 8.663 ms (Phase 1: 9.480); `.slo` median 545.224 ms (Phase 1: 615.369) | |
| CP-b suite (`-LE determinism-sweep`) | first run 1384 tests, 1382 passed (`ShadingScope_Greps`, `Link_RenameAliases` on stale files: section 4, F-7 and F-8); **after the F-7 to F-9 fixes: 1384 tests, 1383 passed in one run and the other, `ShadingParity_JitPL_trace_samples_cone`, hit its timeout under the host's load and passed alone (56.28 s), so 1384 of 1384** | 1385 tests, 1380 passed: the baseline's two (#45, #163), `ShadingScope_Greps` and `Link_RenameAliases` (section 4), and `Link_InfoRuntimeNotCoLinked` on its first run (CMake 3.28 needs policy CMP0057: fixed in the script; the test then passes alone on Linux) |
| CP-d images | 0 differing, 0 missing (65 fresh images, information only); `MaxBlockAvgDiff` rows identical (empty diff) | 0 differing, 0 missing; rows identical |
| CP-c architecture | 42/42; `archcheck` PASS, `ALLOWED DB-004 x 3 DB-011 x 144`, TOTAL 491/71, Shading 0/0; **`CONSUMER` 71 -> 69 (`sloinfo` 3 -> 1: only `ShaderMetadata.h`)**; `oshader`'s 3 edges are its own compiler's headers (`llvmEmitter.h`, `opcodes.h`, `rslo.h`): nothing of `oshader` is this spec's; allowlist and `domain-map.txt` diffs empty | 42/42; same figures (`CONSUMER` 69) |
| CP-m contract tests | 249/249 | 249/249 |
| CP-v | version `{2, 4, 0}`, no `static_assert` edited | same tree |
| Part B: the files | `build/src/libshader/shading`: `libRSLRuntime.1.0.0.dylib`, `.1.dylib`, `.dylib`, and the symlink `libshader_shading.dylib -> libRSLRuntime.dylib`; `build/src/libshader/metadata`: `libRSLInfo.1.0.0.dylib`, `.1.dylib`, `.dylib`, `libRSLInfo.a`, and the symlinks `libshader_runtime.dylib`/`libshader_runtime.a`; `otool -L libRIBRender.dylib` names `libRSLRuntime.1.dylib` | `libRSLRuntime.so{,.1,.1.0.0}` and the link `libshader_shading.so`; `libRSLInfo.so{,.1,.1.0.0}`, `libRSLInfo.a` and the links `libshader_runtime.so`/`.a` |
| `Link_InfoRuntimeNotCoLinked` | green; 356 link lines (the install-layout and package sub-builds are not read), the dependency closure of 340 images; **mutation checks fail as they must**: a scratch link line naming `RSLInfo` and `RSLRuntime`, and a scratch dependency list giving `sloinfo` a second owner | green (359 link lines, 343 images); both mutation checks fail as they must (`.so.1` names) |
| `ShadingContract_StandaloneLinkLine`, `PluginAbi_InstallLayouts` (`install_consumer`, old and new export names), `Link_*`, `Common_StateOwnership` | green with the three approved edits | green (except the two in section 4) |

**The interim state (T127, `build`).** With the CMake rename in and the three test files not yet edited: `ShadingContract_StandaloneLinkLine` red (`shader_shading` absent from the link line, as intended); `PluginAbi_InstallLayouts` green only through the exported alias; `ShadingScope_Greps` **green with the stale `libshader_shading.dir/flags.make` still on disk**, and in `build-nojit` the stale directory is likewise on disk (nothing deleted). See section 4 for what that test really checked.

**The opt-in package test (T131), Linux only (the maintainer's rule: on the Linux host, at the default `CPUJOB_TIMEOUT`).** `-DOPENRENDER_LINK_PACKAGE_TEST=ON`, `ctest -L '^link-package$'` through the remote `cpujob`: `Link_PackageStaticConsumer` **passed in 569.96 s** (the scratch install and the consumer, both formats; no timeout), then the option was set OFF again. It consumes the installed package's Static component (`openRender::RIBRender_static`); it does not name `RSLRuntime`/`RSLInfo`, so the old and new export names are proven by `PluginAbi_InstallLayouts`'s `install_consumer` (green on both platforms, all of `RSLRuntime`, `RSLInfo`, `shader_shading`, `shader_runtime`). Left on the Linux host: `build/tests/link/package_work` (the scratch install and consumer tree); no `build-pkg` was created.

## 2. Red evidence

Phase 1 has none (the characterisations pass on the starting tree by construction).

### Phase 8, T116 and T122: the Test gates

Part A (T116, before the source edits): `ShaderMetadata_Shipped` failed to build (`fatal error: 'libshader/include/openrender/ShaderMetadata.h' file not found`); `Sloinfo_MetadataApiOnly` reported 14 violations in `src/sloinfo` (`rslo.h`, `libshader/shading/llvmJit.h`, `libshader/shading/sloMetadata.h` and the names `rsloGet`, `rsloDelete`, `CLLVMJitEngine`, `SLOShaderInfo`, `TRSLObjectShader`, plus `CLLVMJitEngine` in `src/sloinfo/CMakeLists.txt`'s comment); its fixture run passes by `WILL_FAIL`.
Part B (T122): `Link_RenameAliases` -- "no such target (the renames are not done): RUNTIME;RUNTIME_ALIAS;INFO;INFO_ALIAS;INFO_STATIC;INFO_STATIC_ALIAS;RSLO_ALIAS;RSLO_STATIC_ALIAS;COMPILER", from the script run with every target value empty (the state before the renames).

### Phase 8, T114: the hazard the `check_scope.cmake` edit closes

`ShadingScope_Greps` (S1a, S1e) reads the shading library's include list from `build/src/libshader/shading/CMakeFiles/libshader_shading.dir/flags.make`. After the rename the live target is `libRSLRuntime`, so that file is no longer rewritten. In a **fresh** tree it is absent, and S1a/S1e already fail ("cannot read the shading library's include path"). In an **incremental** tree (`build`, `build-nojit`, the Linux tree) the old `libshader_shading.dir/flags.make` stays on disk, so S1a/S1e stay green while they check the *old* include list and not `libRSLRuntime`'s. The proposed edit (`tools/rename-test-edits.patch`) points the path at `libRSLRuntime.dir/flags.make` and makes an empty include list `FATAL_ERROR`. T127 records the actual outcome per tree before the edit is applied; nothing is deleted (the stale directory is left, and asked about).

### Phase 6, T098: the Test gate

`ShadingSystem_GroupOneLayer` and `ShadingSystem_BindOrder` (gated since Phase 5) moved out of the red block; before any source edit both failed to build: `fatal error: 'libshader/shading/shaderGroup.h' file not found`.

### Phase 5, T081: the Test gate (the tests enabled, before any implementation)

| Test | Red reason (recorded before the first source edit) |
|---|---|
| `ShadingSystem_ThirdBackend`, `_Precedence`, `_Cache` (`test_support.h`) | build failure: `fatal error: 'libshader/shading/shadingSystem.h' file not found` |
| `ShadingSystem_BindOrder` | build failure on `shadingSystem.h`, then (the facade written) `'libshader/shading/shaderGroup.h' file not found`: it binds through `ShaderGroupBuilder`, which is Phase 6, so it stays in its red block with `ShadingSystem_GroupOneLayer` (a departure from the task list, which names it for Phase 5) |
| `ShadingSystem_NoJitFallback` (`build-nojit`) | build failure on `shadingSystem.h` |
| `Shading_CoreNoBackendNames` | 13 `VIOLATION` lines on the Phase 4 tree: `rendererFiles.cpp` 8, `rendererContext.cpp` 2, `rendererFileSearch.cpp` 2, `src/ri/CMakeLists.txt` 1 (the `init.cpp` lines of the 35 were already gone); `Shading_CoreNoBackendNames_Fixture` (the mutant) passes by `WILL_FAIL` |

### Phase 3, T048: the link-red step (the proof that #134's references were hidden in the core)

`init.cpp` was moved with `git mv` to `src/libshader/shading/rsloInit.cpp` and added to `shading_sources` unchanged, with the renderer's header directories temporarily added to the library's include path (the library does not see them, so without that it would not even compile: also a proof; the temporary line was removed again). `libshader_shading` then **fails to link**
(`build-logs/033-p3-red-link.log`):

```text
Undefined symbols for architecture arm64:
  "CRenderer::retrieveVariable(char const*)", referenced from: CRendererContext::init(CProgrammableShaderInstance*) in rsloInit.cpp.o
  "CRenderer::findCoordinateSystem(char const*, float const*&, float const*&, ECoordinateSystem&)", referenced from: CRendererContext::init(...) in rsloInit.cpp.o (19 times)
ld: symbol(s) not found for architecture arm64
```

### Phase 2 (T042 to T044): the option ON, each new target built on its own through `cpujob`, then the option OFF again

Every failure is the stated reason; the same on macOS (clang, `build`, JIT on) and Linux (GCC 13); the JIT-less case on macOS `build-nojit`.
The logs are `build-logs/033-p2-red-*.log` (macOS), `033-p2-red2-*`, `033-p2-red-nojit.log`, `033-l2-red-*.log` (Linux).

| Test | Result | Reason (first lines of the failure) |
|---|---|---|
| `ShadingSystem_ThirdBackend` | build failure | `test_backend.h:21: fatal error: 'libshader/shading/shaderBackend.h' file not found` |
| `ShadingSystem_Precedence` | build failure | `test_support.h:29: fatal error: 'libshader/shading/shaderBackend.h' file not found` |
| `ShadingSystem_Cache` | build failure | the same header |
| `ShadingSystem_BindOrder` | build failure | `test_bind_order.cpp:17: fatal error: 'libshader/shading/shaderGroup.h' file not found` |
| `ShadingSystem_GroupOneLayer` | build failure | `test_group_one_layer.cpp:14: fatal error: 'libshader/shading/shaderGroup.h' file not found` |
| `ShadingSystem_NoJitFallback` (`build-nojit`) | build failure | `test_support.h:29: fatal error: 'libshader/shading/shaderBackend.h' file not found` |
| `ShadingSystem_BindUnknownSpace` | build failure | `error: no member named 'phase' in 'openrender::shading::MatrixRequest'`; `use of undeclared identifier 'EMatrixPhase'`; `use of undeclared identifier 'libshader'` (`libshader::initialiseInstance`) (GCC: `has no member named 'phase'`, `'EMatrixPhase' has not been declared`, `'libshader' has not been declared`) |
| `ShaderMetadata_Shipped` | build failure | `fatal error: 'libshader/include/openrender/ShaderMetadata.h' file not found` |
| `ShadingContract_MessageRequestsBoth` | build failure (the other of the two reds the task allows: it compiles against no stub on the starting tree) | `no type named 'ShaderNameAnswer'`, `no type named 'ShaderNameRequest'`, `no member named 'phase' in 'MatrixRequest'`, `no member named 'EMatrixPhase'`, `no member named 'destBytes' in 'MessageRequest'`, `no member named 'getShaderName' in 'CRendererServices'` |
| `ShadingContract_ShadernameNoServices` | **assertion** (it builds): the actual output on the starting tree, both backends (`Interp`, `Jit`): `Results: 13 passed, 5 failed`; `Ci.y` is 1 where the test expects 0 (`shadername("surface")` returned the real name `cm_producer_surface` read from the bound record, four points) and the warning count is 0 where it expects 1 (`sink.countText("renderer services do not provide shadername()") == 1`: no warning is given today) | |
| `Shading_CoreNoBackendNames` | red, **35** `VIOLATION` lines | exactly `tools/scan-base.txt`: `init.cpp` 19 + 2 (`OPENRENDER_HAVE_LLVM`), `rendererFiles.cpp` 6 + 2, `rendererContext.cpp` 2, `rendererContext.h` 1, `rendererFileSearch.cpp` 2, `src/ri/CMakeLists.txt` 1 |
| `Shading_JitMessagesThroughServices` | red, **14** lines | `rslOps.cpp:924 getParameter`, `:938 getParameterShape`, and 12 `currentAttributes` lines at `shading.cpp:2909` to `2932` (the `shaderName` bodies) |
| `Sloinfo_MetadataApiOnly` | red, **14** lines | 11 parser names (`CLLVMJitEngine`, `TRSLObjectShader`, `rsloGet`, `rsloDelete`, `SLOShaderInfo`) and the 3 includes (`rslo.h`, `libshader/shading/llvmJit.h`, `libshader/shading/sloMetadata.h`) |
| `Link_RenameAliases` | red | `Link_RenameAliases: no such target (the renames are not done): RUNTIME;RUNTIME_ALIAS;INFO;INFO_ALIAS;...` (an absent target is passed as an empty value: a generator expression naming it would stop the configure) |
| the three fixture runs (`..._Fixture`, registered `WILL_FAIL`) | the scan fails as it must (the mutant check): `VIOLATION src/ri/render/planted.txt:2: parseShader`, `VIOLATION src/libshader/shading/rslOps.cpp:2: getParameterShape`, `VIOLATION src/sloinfo/sloinfo.cpp:3: #include "libshader/runtime/rslo.h"` | |

Linux counts of the scans: 35 + 14 + 14 = 63 `VIOLATION` lines, the same as macOS. `ShadingContract_ShadernameNoServices` builds under GCC 13 `-Werror`.

**Syntax check against stubs.** Because a test that cannot compile now is only ever shown failing at its first missing header, the test code behind that first error was also compiled, with `-fsyntax-only`, the project's `-Wall -Wextra -Wpedantic -Werror` flags and a stub copy of the
headers (the contracts' API written out in a scratch directory outside the tree: `ShadingSystem`, `IShaderBackend`, `IShaderFileResolver`, `IShaderBindHost`, `ShaderGroupBuilder`, `groupPrepare`/`groupExecute`, `CShaderPayload`, `CShader::backend`/`payload`, `CShaderInstance::shaderDescription()`,
`EMatrixPhase`, `MatrixRequest::phase`, `MessageRequest::destBytes`, `ShaderNameRequest`/`Answer`, `getShaderName`, `libshader::initialiseInstance`, `ShaderMetadata.h`). All 12 files pass (`build-logs/033-p2-syntax.log`). It proves the tests are well formed against the contracts, not that the later implementation spells the API the same way: a difference is a TEST-APPROVAL item for that phase.

## 3. Linux records

See section 0.3 and 0.4, and the Phase 2 table in section 1. Every push was verified: after the Phase 2 push "4406 files, 0 differences"; after the Phase 1 pulls "8 files, 0 differences". The remote tree has no `.git`.
`linux_sync.sh` is `specs/033-shading-system-backends/tools/linux_sync.sh` (copied from 031's; the remote root is hard-coded to `Projects/Development/CLI/openrender-033`; it
refuses `openrender`, `cellar`, `shrimp`, `openrender-029`, `-030`, `-038`, a malformed `openrender-` and every other root: tested by passing each name, all exit 2; and it aborts, listing
them, when an untracked non-ignored file exists: tested with a temporary file). One addition to 031's script: `pull` also fetches the Linux-recorded references under
`tests/*/reference*/linux/` (it must, they are recorded on Linux).

## 4. Findings for filing and departures from the plan

**Filed by the maintainer after Phase 1 (out of this spec's mandate, nothing was fixed; F-1 is #175, F-2 is #176, F-4 is #177):**

- **F-1 (GitHub #175)**: the `.rslo` loader (`parseShader`, `rslo.y`) and `sloinfo --rslo` reject a **matrix parameter default written in a named space**: `oshader` emits
  `uniform matrix m = "current" [ 1 0 0 0 ... ]` and the parser stops with `syntax error` (the shader is then not loaded: `Error in shader ... (5) ("syntax error")`).
  Point, vector and normal defaults in a named space parse. Reproduce: `surface s(matrix m = matrix "current" (1,0,0,0, 0,1,0,0, 0,0,1,0, 4,5,6,1)) { Ci = 0; }`.
  `BindSpaces_Characterise` therefore leaves matrix defaults out; the plan's "a custom system, e.g. `point p = point "current" (1,2,3)`" is covered for point, vector and normal.
- **F-2 (GitHub #176)**: an intermittent crash (SIGBUS, about 5% of runs on macOS) when the `matrices`, `attributes` and `messages` families of the spec 026 rendered harness
  (`CRenderedSession`, `CContractForwarder`, producers bound to the object's attributes) run **in one RiBegin session**; it occurred at the unknown-coordinate-system row, after
  the attribute rows, and (before a sink was installed) while printing the warning. The existing `RealAnswers*` cases run each family in a process of its own and do not
  show it (0 of 120 runs). `ShadingContract_MessageRequests` therefore runs each family in its own process (600 process runs, 0 failures; Linux 0 of 100). The cause was
  not found (no debugger); it looks like the heap-corruption family of the known flaky `ShadingParity_AmbientAccumulation`, so it is a candidate for the same issue.
- **F-3**: starting-tree Linux failures that Phase 1 recorded as the baseline's own: `TextureTile_RenderByteIdentical` (#45) and the photon rows of `Registry_IntegratorImageIdentity` (#163).
- **F-4 (GitHub #177)**: `Shaders_BytesUnchanged` fails in a JIT-less tree (`build-nojit`): the `oshader_help.txt` reference expects the `--jit` line, which a JIT-less `oshader` does not print. It is
  green with the JIT. A starting-tree property; the JIT-off baseline therefore has this one failure besides the intermittent `Registry_IntegratorImageIdentity`.
- **F-5** (observation, no action needed): the invalid shader-format attribute value is turned into `"slo"` with a warning, so `Attribute "shade" "shaderformat" ["bogus"]` makes `slo` the
  preference, not the compile-time default. `ShaderFormat_Precedence` pins it.

**Departures from the task text:**

- `tests/shaders/references/sloinfo137/` and `tests/shading_contract/reference/` (maintainer-approved at PAUSE 1a). The `Sloinfo_Output137` record is two files (`rslo.txt`, `slo.txt`), not 274.
- `ShadingContract_MessageRequests` runs 6 processes (two backends times three families) for finding F-2; its reference is one file of six sections. The recorded tuples also hold the `getMatrix`
  and `getAttribute` requests (the task says getMessage/getAttribute/getMatrix) with handles as labels (`null`, `obj`, `light`, `exec`).
- The two unapproved first recordings of `precedence.txt` (JIT fixture had a wrong internal shader name, so the `.slo` did not bind) and of `bind_spaces.txt` and `message_requests.txt`
  (the first two included the loader's location notes; the third crashed, F-2) were replaced by re-recording before any approval: they were my own new, never-approved files.
- The timing-script is a new script that follows 031's method, not an edit of its scene list.
- `build-logs/` was created (gitignored) for the job logs.

**Phase 2 departures (all in the maintainer-approved registrations, listed for the review of the written tests):**

- The registration helper is `or033_red_test()` in `tests/CMakeLists.txt` next to the option: with the option ON a normal test; OFF a DISABLED placeholder of the same name, so the everyday run shows "Not Run (Disabled)" for 16 entries
  (the 14 tests, counting `ShadingSystem_NoJitFallback` only in a JIT-less tree, and the three fixture runs of the scans).
- `ShadingSystem_BindUnknownSpace` is one test that runs `rslo` and `slo` in two processes (`check_all_modes.cmake`); it uses only what exists after Phase 3 (`parseShader`/`parseSloShader` through the harness, `libshader::initialiseInstance()`, the
  services double), not the Phase 4 facade, because Phase 3 enables it. It does not call `libshader::bindCoordinateSystem()` directly: the argument types of that function are not fixed by the contract; the request is observed through the services (`phase == Bind`, name `nosuch`).
- `ShadingContract_MessageRequestsBoth` runs one backend per process and family (F-2), and "equal across backends" is checked against the OTHER backend's section of `tests/shading_contract/reference/message_requests.txt` (Phase 1 recorded the two sections identical); the shadername row is left out of that comparison and asserted through `getShaderName`.
  Its body repeats `case_message_requests.cpp` with the output collected in a string, because that file is an approved characterisation and is not edited.
- `ShadingSystem_Precedence` builds the attribute value `"bogus"` with `CAttributes::setShaderFormat`, as the core does (it becomes `"slo"`), so the table is the recorded one.
- The scans take `-DSOURCE_DIR=<a tree holding src/...>` so the fixture runs scan `tests/architecture/fixtures/033-*` (the fixture files are `.txt`/`.cpp` fragments that no target compiles); `Sloinfo_MetadataApiOnly` also flags the include `"rslo.h"` (the inspection parser's header, reached through the include path), which is the third of the three `CONSUMER` edges.
- `Link_RenameAliases` passes an absent target as an empty value instead of a generator expression, so the option ON can configure; the script then reports "no such target". Registered whether or not the option is ON, DISABLED when OFF.
- The `ShadingSystem_*` tests other than `NoJitFallback` and the tests that need `.slo` are registered in trees with the JIT only; `NoJitFallback` in trees without it.

**Departure, Phase 5: `libshader::initialiseInstance()` stays.** The approved list removed it, but the approved, committed test `ShadingSystem_BindUnknownSpace` (`test_bind_unknown_space.cpp:65`) calls it on a bare instance, and an approved test is not edited. It is the same one-line forward as before (`instance.parent->backend->initialise(instance)`), no longer called by the core; `ShadingSystem::bind()` calls the backend's `initialise()` directly. If the maintainer wants it gone, that test needs a one-line edit (to be shown as a diff first).

**Phase 8 findings and departures (nothing below was fixed beyond the approved edits; the first two need the maintainer):**

- **F-7 (`ShadingScope_Greps` S1a/S1e were vacuous): resolved, option A (maintainer-approved 2026-11-17).** `check_scope.cmake` read the shading library's include list with
  `if(_t MATCHES "^-I(.*)$" OR _t MATCHES "^-isystem(.*)$")` and then `${CMAKE_MATCH_1}`: both operands of an `OR` are evaluated, the second fails and clears `CMAKE_MATCH_1`, so
  `_include_dirs` stayed empty (CMake 4.4.5 on macOS; the Linux run read nothing either) and S1a ("`src/ri/hiders` is not on the include path") and S1e (the same for `src/ri/geometry`) passed on
  any list. The approved `FATAL_ERROR` on an empty list turned that into red in every tree. The real list did contain `src/ri/hiders` and `src/ri/geometry` (a stale `libshader_shading.dir/flags.make`
  from a 2026-11-14 tree listed them too, so it predates this spec; `DOMAIN_BOUNDARIES.md` carries the dated re-measure note). Fix: (1) the parse reads the directory in the branch that matched
  (`if(-I) ... elseif(-isystem) ...`; the only edit to `check_scope.cmake` beyond `rename-test-edits.patch`); (2) `src/libshader/shading/CMakeLists.txt` drops exactly `src/ri/hiders` and
  `src/ri/geometry` from its directory's include path, the way the compiler's CMake drops the renderer's (a per-directory property, no other target affected). The filtered build compiled
  without a single change to a source file (the 28 shading objects rebuilt; macOS `build` and `build-nojit`). `libRSLRuntime`'s flags.make then lists none of the two; the script reads 17
  include directories. **T128 mutation proof:** planting `#include "ri/hiders/hider.h"`, `#include "ri/geometry/surface.h"` and `#include "object.h"` into `src/libshader/shading/shaderGroup.cpp`
  (restored byte for byte; the staged file is unchanged) makes `ShadingScope_Greps` fail with `FAIL S1a: ... includes ri/hiders/hider.h`, `FAIL S1e: ... includes ri/geometry/surface.h` and
  `FAIL S1e: ... includes object.h`; S1b-S1d still pass. The stale `flags.make` directories were removed first (F-8), so the proof ran with the include list read from `libRSLRuntime.dir`.
- **F-8 (stale old-name build outputs): removed (maintainer-approved 2026-11-17), exactly this list.** macOS `build` and `build-nojit`, `src/libshader/shading/`: `libshader_shading.1.0.0.dylib`, `libshader_shading.1.dylib`,
  `libshader_jitmeta.a` (`build` only), `CMakeFiles/libshader_shading.dir`, `CMakeFiles/libshader_jitmeta.dir` (`build` only); `src/libshader/runtime/`: `libshader_runtime.1.0.0.dylib`, `libshader_runtime.1.dylib`,
  `libshader_runtime.a`, `libshader_runtime.dylib`, `CMakeFiles/libshader_runtime.dir`, `libshader_runtime_obj.dir`, `libshader_runtime_static.dir`. Kept: the compat symlinks `libshader_shading.dylib -> libRSLRuntime.dylib`
  (shading directory) and `libshader_runtime.dylib`/`.a` beside `libRSLInfo` (metadata directory). `Link_RenameAliases` is green in both trees afterwards. The Linux tree is done after spec 040's closing chain on the host: **pending** (the Linux column above is the run before the F-7 to F-9 fixes: `ShadingScope_Greps` and `Link_RenameAliases` red there for the reasons given; the re-run, the same removal list and the three affected tests follow).
- **F-9 (`.gitignore` hid the new header): resolved (maintainer-approved).** Line 43 is now `/openrender/`; the ignored-file list of the worktree is unchanged, `git status` shows no new untracked file, the main checkout has no other
  directory of that name outside `src/libshader/include/openrender/` (tracked), and `check-ignore` no longer matches the header (still tracked).
- **My registration of `Link_RenameAliases` (Phase 2) could never see the targets.** It tested `if(TARGET libRSLRuntime)` at configure time, but `tests/` is configured before `src/`
  (`CMakeLists.txt`), so every value was empty and the test could only ever report "no such target". It now passes `$<$<TARGET_EXISTS:t>:$<TARGET_FILE:t>>` (generate time). The script itself is unchanged.
- **Departures from the task text:** `rslo.h` and `common/rendererc.h` spell the same variable-type and container enumerators in one global scope, so `rsloMetadataReader.cpp` renames them for the length
  of the `rslo.h` include (`RSLO_TYPE_*`, `RSLO_CONTAINER_*`); the `.rslo` default for a missing string is "NULL" and a missing vector or matrix is zeros (what the listing printed); `sloinfo` keeps its own
  format detection by magic and learns "no `.slo` reader" from `EMetadataStatus::Unsupported` instead of `#ifdef OPENRENDER_HAVE_LLVM`; `Link_InfoRuntimeNotCoLinked` skips the install-layout and package
  sub-builds (copies, made and removed while other tests run) and sets policy CMP0057 (CMake 3.28 on Linux); the metadata, install and rename docs are edited as T132 lists, `DEVNOTES.md` rows are
  history rows of earlier specs and are left as written, and `BINDINGS_GUIDE.md`, `src/python` and `src/lua` name no library (nothing to change).

## 5. Final summary against SC-001 to SC-011

Phase 9.
