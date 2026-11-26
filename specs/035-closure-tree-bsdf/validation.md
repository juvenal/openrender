# Validation record: spec 035 (closure tree and BSDF)

Appended at every checkpoint. Commands, counts, wall times, what is red and why.

## Phase 0

### Sync, pre-flight (T001, T002)

- T001: the maintainer committed the staged spec documents; the coordinator prepared a no-commit merge of master into the branch. `MERGE_HEAD` = `e1737ae54e86fb140c70a878241a7f839b9fc299` ("Merge branch 'fix/205-dof-shading-rate-focusfactor'"), no conflicts. Every "master `4ee81900`" in the documents reads "current master" (`e1737ae5`).
- `git log master -1`: `e1737ae54e86fb140c70a878241a7f839b9fc299 Merge branch 'fix/205-dof-shading-rate-focusfactor'`.
- `kRendererServicesVersion` = **2.6.1** (`src/libshader/shading/rendererServices.h:180`), unchanged by #205 (it adds a `getAttribute` name, no member).
- `grep -rn "closuremode\|OrClosure\|RiBxdf" src tests/CMakeLists.txt`: nothing (no name collision after the sync).
- macOS: `df -h /Volumes/Projects` 14 GiB free; `sysctl vm.swapusage`: **7.9-8.1 GB of 9.2 GB used** (reported to the coordinator: swap is in use, other specs' jobs hold the lock); the cpujob lock was held by other specs' jobs (`fix16-201-build`, `054-p0-build`, `054-p0-all`) so every job of this phase queued first.
- Linux: `df -h $HOME` 9.5 GiB free of 14 GiB on `/home` (the 035 tree did not exist; the host's other trees: `openrender`, `openrender-029`, `openrender-038`, `cellar`, `shrimp`, `free`); remote `cpujob --status` free.
- **Finding (T003)**: the worktree had **no `build/` directory** (tasks.md T003 says "an existing `$WT/build` is reused"). A from-scratch configure was run, `~/.claude/bin/cpujob 035-p0-cfg cmake -B build -S . -DCMAKE_BUILD_TYPE=Release` (the options of the main tree: JIT ON, OIIO OFF; vcpkg through `VCPKG_ROOT` / `VCPKG_INSTALLED_DIR` from the environment, shared `~/.cache/vcpkg-installed/openrender`; LLVM 23.1.1, OpenEXR 3.4.15). Configure log: `build/p0-cfg.log`; 137 `.sl` -> `.rslo` + `.slo`. The first build is therefore a full build.

### Spec documents against the synced tree (T013)

All eight approved corrections are in place:

1. `data-model.md:121` (`OrBxdfTable.create(const void *const *values, const OrBxdfDiagnostics *)`, `build(..., const OrClosureAllocator *)`) and `:86` (`OR_CPARAM_*`) follow `contracts/bxdf.md:37,44` and `contracts/closure-abi.md:147`.
2. `plan.md:74`: "24 new source files under `src/libshader/closures/` ... and 1 under `src/common/`".
3. `research.md:476-481` (R23) is the 2026-11-23 text. It names master `4ee81900`; read as current master. **It does not yet mention #205** (see "New since the documents" below); not edited (no PAUSE).
4. `spec.md:260` (FR-019): `indirectspecular` is "not a builtin of this tree".
5. `spec.md:309` (SC-007): the isolation of the `closures` object is stated.
6. `cldbl` delegates `execute` to the real backends: `plan.md:52,165,268`, `research.md:295`.
7. The sweep's coverage as the interim behaviours only: `spec.md:304` (SC-006, Q15 row `:38`).
8. Services starting point 2.6.1 everywhere (`tasks.md:5`; the other `2.6.0` hits are history notes).

**New since the documents (to report, not edited)**: #205 (`e1737ae5`, no services step). It adds the token `RI_FOCUSFACTOR` ("focusfactor": `riTokens.cpp:273`, `riVocabulary.h:336`, `ri.h:390`), `CAttributes::focusFactor` (`attributes.h:187`, init `attributes.cpp:122`, which shifts every later line of `attributes.cpp` by +1), `RiGeometricApproximation`'s `focusfactor` branch (`rendererContext.cpp:2738`), the services name `"GeometricApproximation:focusfactor"` (`rendererShadingServices.cpp:188,297`), and the `FOCALBLUR` block of `CPatch::probe` (`surface.cpp:437`). Phase 4's `RiBxdf` and Phase 3's `closuremode` token add their lines in the same files (`ri.h`, `riTokens.cpp`, `riVocabulary.h`, `attributes.*`, `rendererContext.cpp`): no conflict, they are separate lines.

### Classification (T012)

`tests/architecture/domain-map.txt:63` `prefix  src/libshader/  Shading` covers `src/libshader/closures/`; `:18` `prefix  src/common/  Infra` covers `src/common/spectrum.h`. Nothing else under either prefix overrides them (`:66` is `src/libshader/tests/  Consumer`). No map line is needed.

### Funnels re-read on the synced tree (T010)

Master `b9dec4e5` -> synced `e1737ae5`. All in `src/libshader/shading/` (`shading.cpp` unless a file is named).

| Symbol | b9dec4e5 | synced | Result |
|---|---|---|---|
| `iterateLights` (non-`saveCat` overload, forwards) | 2049 | 2050 | confirmed |
| `iterateLights` (`saveCat` overload) | 2053 | 2054 | confirmed; the one place every light shader of `lightSources` with `SHADERFLAGS_NONAMBIENT` runs (`:2089` loop; bound area light through `iterateBoundAreaLight` `:2117`) |
| `iterateBoundAreaLight` | 2142 | 2143 | confirmed; reached only from `iterateLights` (`:2117`) |
| `prepareAmbient` | 2508 | 2518 | confirmed, **but it is not the only ambient funnel** (see Finding A) |
| `callAmbient` | 2267 | 2268 | **new funnel, Finding A** |
| `prepareDiffuse` | 2545 | 2555 | confirmed (calls `iterateLights` `:2563`) |
| `setupIlluminance` | 2560 | 2570 | confirmed (calls `iterateLights` `:2578`) |
| `callDiffuse` | 2323 | 2324 | confirmed (`iterateLights` `:2336`; #197 hunk in it at 2345-2368 does not touch the loop) |
| `callSpecular` | 2377 | 2387 | confirmed (`iterateLights` `:2400`) |
| `callPhong` | 2439 | 2449 | confirmed (`iterateLights` `:2461`) |
| `jitIlluminanceBegin` | 2974 | 3005 | confirmed with #200's category argument: it still ends in `iterateLights(..., saveCat, ...)` (`:3046`); `op_illuminance_begin_v3` is at `rslOps.cpp:1582` (new), `v2` stays for the forms without a category |
| `jitIlluminateEnd` | 2688 | 2717 | confirmed; #197's hunk at 2756 saves the mutable outputs (inside a light shader, which never runs under the gate) |
| `jitSolarEnd` | 2841 | 2871 | confirmed; #197 hunk at 2908 |
| `jitIlluminate3Begin` | 2638 | 2648 | confirmed |
| `gatherHeaderBegin` | 4907 | 5006 | confirmed (declared `shading.h:526`; `GATHERHEADEREXPR_PRE` `giFunctions.h:379` calls it, and so does the JIT's `jitGatherHeaderBegin` `:5111`) |
| `jitOcclusionBatch` | 3835 | 3875 | confirmed |
| `jitOcclusion` / `jitIndirectDiffuse` | 3934 / 3940 | 3974 / 3980 | confirmed (thin forwarders to `jitOcclusionBatch`) |
| `libshader::irradianceLookup` (`irradianceCalls.cpp`) | 42 | 42 | confirmed; **the point-based branch is inside it** (`:56-62`), see Finding B |
| `IDEXPR` (`giFunctions.h`) | 230 | 230 | confirmed (calls `irradianceLookup` `:236`; `DRAW_BIND` before it) |
| `lightsource` interpreter (`shaderFunctions.h`, `DEFFUNC(Lightsource...)`) | 1308 | 1308 | confirmed; it reads `currentLight`, which the gate leaves empty |
| `lightsource` JIT: `op_lightsource_param` (`rslOps.cpp`) / `jitLightsourceParameter` (`shading.cpp`) | n/a | 921 (declared `rslOps.h`), 2247 / 4264 | new in #115; reads the iterated light |
| `getMessage` (Light source, #115) in `rendererShadingServices.cpp` | 480 | 484 | confirmed (RISpec 15.8 same-type rule) |

**Finding A (reported before Phase 3; a funnel gained callers).** `closuremode.md` "The lighting gate" gates `ambient()` at `prepareAmbient` alone. In this tree, and already at `b9dec4e5`, the ambient lights run in **three** places, each with its own `for (... lightSources ...)` loop over the lights without `SHADERFLAGS_NONAMBIENT`:
 1. `CShadingContext::prepareAmbient()` (`shading.cpp:2518`, loop `:2538`), called by `rsl_ambient_1v` (`rslBuiltins.cpp:240`);
 2. `CShadingContext::callAmbient()` (`shading.cpp:2268`, loop `:2296`), the batch JIT path (`rslBuiltins.cpp:231`, `rslOps.cpp:797`, `sloBackend.cpp:160`);
 3. the interpreter's ambient macro (`execute.cpp:406`, loop `:425`).
The three set `ambientLightsExecuted` and fill `alights`. A gate at `prepareAmbient` alone would leave `ambient()` ungated in the interpreter and in `callAmbient`. **Decided (maintainer, 2026-11-24)**: one shared inline check that all three sites call; `contracts/closuremode.md` (the lighting gate table and notes), `research.md` R9, `plan.md` (line of the `shading.cpp` row and the summary), `data-model.md` (`lightingSuppressed`), `contracts/consumer-and-backend.md` and `tasks.md` T073 and T077 now say so. Nothing under `src` was edited in Phase 0.

**Finding B (a funnel narrower than described).** The "point-based branches of `IDEXPR` and `jitOcclusion`/`jitIndirectDiffuse`" are not in those: both call `libshader::irradianceLookup`, whose point-based branch is `irradianceCalls.cpp:56-62`. **Decided (2026-11-24): the GI gate stays at the top of `irradianceLookup`.** One gate at the top of `irradianceLookup` covers `occlusion()` and `indirectdiffuse()` in both backends; the draw binding (`DRAW_BIND` in `IDEXPR`, `bindDraws` in `jitOcclusionBatch`) runs **before** the call, so under the gate the sub-stream is bound but unused (the cursor effect is not a lookup).

**Read-only dependants (E11)**, read, not edited. Master's changes since `b9dec4e5` in them: `sloLoader.cpp` (#197: a writable parameter is `STORAGE_MUTABLEPARAMETER`, `numGlobals = numMutable`); `sloBackend.cpp` (+3 lines, `currentLocals` saved around the light call, #197); `rendererShadingServices.cpp` (#115 message rule, #205 name); `rendererServices.h` (2.6.1 entry, `:180`); `rslOps.{h,cpp}` (`op_lightsource_param`, `op_illuminance_begin_v3`); `llvmEmitter.cpp` (#200 category operand, #115 `lightsource` emission, 74 lines). None changes the `execute` seam `cldbl` delegates to; the stale R9 numbers (`:2053`, `:2508`, `:4907`) are now `:2054`, `:2518`, `:5006`.

### `shade()` and the other files plan.md quotes (T011)

`shading.cpp`'s `shade()` is unchanged: master's first `shading.cpp` hunk after `b9dec4e5` is at line 1843 (`restoreState`), after `shade()`. Confirmed by reading the lines: shader selection `:1159-1198` (`usedParameters |=` at `:1198`), `groupPrepare` surface `:1290`, `SHADING_2D` test `:1319`, surface block `:1553-1592` (`groupExecute` `:1556`), camera hit-mode override `:1606`, `memEnd(threadMemory)` `:1617`: **all equal to plan.md**. Spec 054's `shade()` edits are not in the synced tree (`git log master -5 -- src/libshader/shading/shading.cpp` shows #115, #197, #200 only).

| Site | b9dec4e5 | synced |
|---|---|---|
| `RiWorldBegin` (auto-bind `defaultsurface`, `setDefaultSurface`) `rendererContext.cpp` | 937 (`:995-1000`) | 937 (`:995-1000`) |
| `RiOptionV` `rendererContext.cpp`; the `shaderformat` branch | 1713; 2106-2111 | 1713; 2106-2111 |
| `RiAttributeV` `rendererContext.cpp`; its `RI_SHADE` block | 3539; 3923 | 3556; **3940** (plan.md `:3923` is now `:3940`) |
| `RiGeometricApproximation` | 2707 | 2721 |
| `renderer.cpp` `setSamplingConfig` loop in `beginFrame()` | 854 | 854 |
| `shaderParameters.cpp` `usedShaderParameters` | 31 | 31 |
| `attributes.h` `governingMaterial()` / `bxdf()` comment | 154 / 144 | 155 / 145 |
| `attributes.cpp` `bindBxdf` / `restore` / `find` | 334 / 451 / 541 | 335 / 452 / 542 |
| `attributes.cpp` `shaderFormat` sites quoted by plan.md as copy `:265`, `restore` `:503`, `find` `:721` | 265, 503, 721 | **266, 504, 722** |

Corrections to plan.md's table: the `attributes.cpp` numbers (`:265`, `:503`, `:721`) become `:266`, `:504`, `:722`, the `RI_SHADE` block `:3923` becomes `:3940`, `:144` (`bxdf()` comment) becomes `:145`. Nothing else differs.

### macOS baseline (T003 to T006), tree = merge of `e1737ae5` into the branch, no source change by 035

- **T003 build**: `~/.claude/bin/cpujob 035-p0-build cmake --build build --config Release -j4`, from scratch (configure `035-p0-cfg`, log `build/p0-cfg.log`; build log `build/p0-build.log`). Wall time 5 min 15 s of job time (19:48:07 to 19:53:22; the job queued behind other specs' jobs from 19:42), rc 0. 137 `.rslo` and 137 `.slo` in `build/shaders`. The log's warnings are all linker notes (macOS-13.0 against 13.3 dylibs, duplicate libraries): no compiler warning.
- **T004 architecture baseline**: `035-p0-arch`, 42 s, **45 of 45 passed** (`build/p0-arch.log`). Byte copies of `allowlist.txt` (sha256 `fe32159c6e32a59462663b112b43e15a9377460c1fe763219b7b6b7503e041eb`), `ribvector-core-residual.txt` (`0b3bf39655fd07387c7bfa44f0fa3240af37e89fe4cc6d96b4ffb13df28f5e2e`) and `metrics-baseline.txt` (`2ab81f7d33d08242d915291f7daa36c493a5534f84c71c887f0fa9a863cc86e8`) are in `build/p0-baselines/` (build directory, not staged). The Shading `CRenderer::` reference count is **0** (`metrics-baseline.txt`: `Shading 0 0`; Hiders 489 / 69, TOTAL 491 / 71, Geometry 2 / 2).
- **T005 full run**: `035-p0-all ctest -LE 'determinism-sweep|gi_determinism_long' -j4`, ctest wall 404.7 s (job 19:54:30 to 20:01:15): **1745 tests, 1743 passed, 2 failed** (`build/p0-all.log`). `ctest -N` lists 2509 tests in all; `-N -L '^determinism-sweep$'`: **761** entries (as expected).
  - Failing set: `ShadingParity_JitPLEngaged_envshadow_width_slo` (1888) and `ShadingParity_JitPLEngaged_envshadow_filter_slo` (1897): `orender failed with exit code 10` in 0.04 to 0.08 s, running `build/tests/visual/jit-texture/jittex-envshadow-{base,width}-slo.rib`. Neither was in the known intermittent set; **filed as GitHub #225 (P1)**, "ShadingParity_JitPLEngaged_envshadow_{width,filter}_slo fail intermittently: orender exits 10", and treated as known intermittents in later runs (each one seen is still reported). **They are intermittent**: re-run in isolation (`035-p0-rerun`, -j2) width failed again, filter passed; then five repeats of the pair (`035-p0-rerun2`, `035-p0-rerun3`) all passed (width failed 2 of 7 runs, filter 1 of 7). Reported, not worked around, nothing changed. None of the known intermittents (`ImagerRegressionRender`, `Parity_transparency-matte-aov`, `Parity_depthdefault`, `ShadingParity_AmbientAccumulation`, `ShadingContract_RealAnswersInterp_Messages`, `ShadingContract_MessageRequests`) failed in this run.
- **T006 byte-identity oracle** (FR-027), `-t:1`, `~/.claude/bin/cpujob 035-p0-oracle`, run from `build/p0-baselines/oracle/` (not staged), environment: `SHADERS=$WT/build/tests/arealight/shaders:$WT/build/shaders` (the visual suite's own value for the arealight scenes; `camera-dof.rib` needs only the second), `ORENDERHOME=$WT/build/tests/visual/_home`, `DISPLAYS=$WT/build/src/display/{file,rgbe,framebuffer}` colon-joined, `GEOMETRIES=$WT/geometry`. Scenes: `examples/rib/camera-dof.rib`, and, from `examples/rib/tests/`, `arealight-softshadow-quad-raytrace.rib` (`.rslo`) and `arealight-softshadow-quad-raytrace-slo.rib` (`.slo`; the tree has no `arealight-*` scene with that exact name shape beyond these; the task text's `arealight-*.rib` is taken as these two). Two re-renders (camera-dof and the `.slo` scene) reproduced the hashes. SHA-256:
  - `camera-dof.tif` `52beb3b9419db88ee01a526a56ca111b94f743fb9fad5f745b1daec5840e405d`
  - `arealight-softshadow-quad-raytrace.tif` (`.rslo`) `545b3ed2d559b6201d83adfc4834786c7d3a17700822bf11c4d2b55d564f9b2b`
  - `arealight-softshadow-quad-raytrace-slo.tif` (`.slo`) `ff8d98f29c6c9048877a17d9833f9721d5a49cfd9b844871835d70e5dd48757b`
  - The first attempt at the two arealight scenes failed with `Failed to find shader "al_surface_matte"` (exit 255): the test-local shaders are in `build/tests/arealight/shaders`, which the task's render environment line does not list. The hashes above are from the corrected environment.
- The cpujob lock was contended throughout (specs 054 and a bug-fix build held it); swap stayed at about 8.0 of 9.2 GB used through every macOS job.

### Linux (T007 to T009), GCC 13 `-Werror`, Ubuntu 24.04 (2 cores, 5.6 GB), kernel 7.0.0-43

- T007 `tools/sync_linux.sh` ran once (5203 files verified into `/home/juvenal/Projects/Development/CLI/openrender-035`, 81 s). No other tree was touched.
- T008: the first attempt of the configure was denied by the permission classifier; the maintainer approved it and it ran exactly as tasks.md says, through the remote cpujob, except that `CPUJOB_TIMEOUT=7200` was **not** set (standing rule: never set `CPUJOB_TIMEOUT`; the default 2700 s was enough). `~/.claude/bin/cpujob 035-lx-cfg cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DINSTALL_SELFCONTAINED=OFF`: rc 0 (configure 31 s). `035-p0-lx-build cmake --build build -j2`: rc 0, **about 9 min of job time** (00:14:01 to about 00:23), 137 `.rslo` and 137 `.slo`. One compiler warning, in a test: `tests/arealight/test_photon_smoke.cpp:124` ignores the return value of `system()` (`warn_unused_result`, not an error: tests are not built with `-Werror` for it). Not touched (spec 057's file).
- T009 architecture: `035-p0-lx-arch`, 45 of 45 passed (17 s); again after the full run, 45 of 45.
- T009 full run: `035-p0-lx-test ctest -LE 'determinism-sweep|gi_determinism_long' -j2`, ctest wall **813.9 s** (00:24:00 to 00:37:34): **1746 tests, 1744 passed, 2 failed** (`p0-lx-all.log` in the remote tree). `ctest -N`: 2510 tests in all (macOS 2509), sweep **761** entries (the same as macOS).
  - `TextureTile_RenderByteIdentical`: the known failure of the starting tree (GitHub #45).
  - `Visual_sphere-texture3d-bake3d-reyes` (#95): a segmentation fault in `orender` (`Segmentation fault (core dumped)`, 2.25 s), the intermittent texture3d `bake3d` crash recorded for specs 048 and later (GitHub #122). Re-run alone three times (`035-p0-lx-rerun`): passed 3 of 3. Reported, not worked around.
  - The two `ShadingParity_JitPLEngaged_envshadow_*_slo` tests (GitHub #225) did not fail on Linux.

### End of phase architecture check (T014)

`035-p0-arch2`: 45 of 45 passed; `allowlist.txt`, `ribvector-core-residual.txt` and `metrics-baseline.txt` byte-identical to the T004 copies; `git status --short tests src/libshader/tests` shows no changed `reference` file.

### Document corrections applied (maintainer, 2026-11-24; documents only)

- `plan.md`: the `shading.cpp` row `:2053`, `:2508`, `:4907` became `:2054`, `:2518`, `:5006`; the `attributes.{h,cpp}` row `:265`, `:503`, `:721`, `:144` became `:266`, `:504`, `:722`, `:145`; the `RiAttributeV` `RI_SHADE` block `:3923` became `:3940`.
- `spec.md` line 83 (`rendererContext.cpp:3923`) became `:3940` (the same site).
- `research.md` R9: `shading.cpp:2053`, `:2508`, `:4907` became `:2054`, `:2518`, `:5006`; R23: the stale master hash `4ee81900` became `e1737ae5`, the base 035 synced to. R23's merged list does not name #205; not added.
- The ambient gate and the GI gate wording: see findings A and B above.
- Not changed: `tasks.md` still says "master `4ee81900`" in its header, Conventions and T010 (history notes about the sync; read as current master), and the stale R9 numbers it quotes in T010 are quoted as stale.

## Phase 1

### Red evidence (T016 to T023)

Tests written before any implementation (T016 to T022), then `~/.claude/bin/cpujob 035-p1-redcfg cmake -S $WT -B $WT/build -DOPENRENDER_035_RED_TESTS=ON` (rc 0) and one `035-p1-red cmake --build $WT/build --target <t> -j4` per target (swap free 1.08 GiB before each). Every target fails to compile, on the first missing piece (logs `build/p1-red-<target>.log`). Only the first missing header is reported per translation unit: further errors in a test (a typo of mine, a signature that differs from what the implementation will have) show only when the headers exist, so the green run of T030 is the first compile of the test bodies.

| Ctest tests (label `closures` plus) | Target | First failure |
|---|---|---|
| `Spectrum_Basics` (`common`) | `test_spectrum` | `tests/unit/common/test_spectrum.cpp:13: 'common/spectrum.h' file not found` |
| `Closure_Layout` | `test_closure_layout` | `layout_c17.c:8` and `test_layout.cpp:16: 'closureAbi.h' file not found` |
| `Closure_Registry_{Builtins,Alias,Unknown,Refusals,Open,Race,NoLock}` | `test_closure_registry` | `test_registry.cpp:24: 'closureAbi.h' file not found` |
| `Closure_Flatten_{Random,Nested,Collapse,Refusals}` | `test_closure_flatten` | `test_flatten.cpp:25: 'closureAbi.h' file not found` |
| `Closure_Arena_{Release,Copy,Budget,Point}` | `test_closure_arena` | `test_arena.cpp:23: 'closureAbi.h' file not found` |
| `Closure_Accessors` | `test_closure_accessors` | `test_accessors.cpp:13: 'closureAbi.h' file not found` |

The tests live in `tests/closures/` (`CMakeLists.txt`, `test_support.h`, `test_layout.cpp`, `layout_c17.c`, `test_registry.cpp`, `test_flatten.cpp`, `test_arena.cpp`, `test_accessors.cpp`) and `tests/unit/common/test_spectrum.cpp`; `tests/CMakeLists.txt` gained the option and the `add_subdirectory(closures)` (the only edit of that file). All six executables are inside `if(OPENRENDER_035_RED_TESTS)`; nothing is registered with the option OFF (so no existing test changes). The build tree is left configured with the option ON until T029.

### Approval (T024), 2026-11-24

The maintainer approved the Phase 1 tests and the pinned API (recorded in tasks.md under T024 and in `contracts/closure-abi.md` / `data-model.md`), with two corrections that were applied before T030: (7) `closureRegistryLockCount()` is declared in the internal `closureRegistry.h`, not in `closureAbi.h`; (9) the built-in parameter blocks are defined once in the internal `closureBuiltins.h`, and the tests include it instead of mirroring the structs. The approved API, in one place: `openrender::Spectrum`; the accumulated-weight rule (root exactly 1; a child's is its parent's times the parent's own weight, one multiplication per node, root to leaf; a lobe's weight is that times the component's `w`); `ClosureTreeBuilder(CMemPage *&, int numRealVertices, IClosureDiagnostics *)` with a null hook allowed; `closureEvaluateGeneric(tree, visit)` as the Phase 1 walk (Phase 2 puts the BSDF query on top); `ClosureDiagKind` (`BadWeight`, `BudgetExceeded`, `UnknownClosure`, `BadBlockSize`, `BadRefractionIndex`, `PointOutOfRange`), `IClosureDiagnostics`, `ClosureFrameDiagnostics` (once per kind per frame; `CODE_LIMIT` for the budget, `CODE_RANGE` for a bad weight); after a budget overflow the next `setRoot` leaves its point empty.

### Implementation (T025 to T029)

New files: `src/common/spectrum.h`; `src/libshader/closures/{closureAbi.h, closureBuiltins.h, closureRegistry.{h,cpp}, closureTree.{h,cpp}}`. Edits of existing files: `src/libshader/shading/CMakeLists.txt` (the two closure sources listed explicitly, and `src/libshader/closures` and `src/include` on the library's private include path; the Hiders'/Geometry filter above it is untouched), `tests/CMakeLists.txt` (T016), `tests/closures/CMakeLists.txt` (the Phase 1 tests moved out of the red block). `src/libshader/closures/` includes nothing from `src/ri/` (`-L architecture` 45 of 45). The new files in `src` were clang-formatted (they are new, so there was no "clean before" state); the tests were not reformatted.

Two things found by running the tests, both mine:
1. `test_registry.cpp` did not compile: splitting the program into sections left `okParam` used in `sectionOpen` and defined in another section. One declaration was added to `sectionOpen` (no check changed). This is the first compile of the test bodies, as predicted in the red evidence.
2. **A real race in the registry, found by `Closure_Registry_Race`** (it failed once in 8 runs, then passed alone 6 times): `published()` tested `current != nullptr`, which is true as soon as the first built-in is added during the first-use fill, so a second thread skipped the `call_once`, registered at once and took an id below 8 while the fill was still running. Fixed with a separate `ready` flag that is set only after the whole fill. After the fix `Closure_Registry_*` passed 300 repeats in a row (`ctest --repeat until-fail:300 -j4`).

### Build and test (T030), macOS

`035-p1-cfg` (red option back OFF), `035-p1-build*` rc 0 (incremental; the closure sources and the tests compile with `-Werror`, no warning). `-L 'closures|common'`: **36 of 37 pass**; the one failure is `Closure_Flatten_Random`, which is the **PAUSE below**. `-L 'link|shading_system|libshader|shading_parity|architecture'`: **462 of 462 pass** (121 s), so the library the sources joined still links self-contained (`Link_*`, `Link_InfoRuntimeNotCoLinked`, `ShadingScope_Greps`).

**PAUSE: `Closure_Flatten_Random` population floors.** All 1000 per-tree equality checks pass (flatten, the generic walk and the independent reference agree bit for bit). Only two sanity floors fail: `totalLobes > 3000` and `nonEmpty > 900`, which I guessed before measuring; measured 1865 lobes and 895 non-empty trees (a one-off print, reverted). They guard against a degenerate generator and are not numerical tolerances, but changing them lowers an approved test's floor, so it is not done: the proposal and the alternative are in `specs/035-closure-tree-bsdf/tools/proposed/test_flatten_random_floors.md`. The test stays red until the maintainer decides.

### Capability to fail (T031)

Each scratch edit was reverted afterwards (`git diff --stat` empty) and the tree rebuilt:

| # | Scratch edit | Result |
|---|---|---|
| 1 | `closureEvaluateGeneric`'s walk visits B before A | `Closure_Flatten_Nested` and `Closure_Flatten_Random` fail (rebuild + `ctest -R 'Closure_Flatten|Closure_Accessors'`: 2 of 5 fail; Nested is the clean evidence, Random is red for the floors as well); `Closure_Accessors` stays green (order-free) |
| 2 | `OrClosureMul`'s members reordered (`closure` before `weight`) in `closureAbi.h` | `test_closure_layout` does not compile: `static assertion failed ... sizeof(OrClosureMul) == 24` and `offsetof(OrClosureMul, weight) == 4`, at `closureAbi.h:180-181` (and in the test's own assertions) |
| 3 | the `emission` deprecation `call_once` removed | `Closure_Registry_Alias` fails: `sink.countContaining("emission") == 1: 3 != 1` |
| 4 | the budget check dropped (`if (false && ...)`) | `Closure_Arena_Budget` fails on 20 checks (bytesUsed over the budget, no `CODE_LIMIT`, the bump pointer past the budget, the root not empty, ...) |

### Linux (T032), GCC 13 `-Werror`

`tools/sync_linux.sh` (5221 files verified), `035-p1-lx-cfg`, `035-p1-lx-build` and `035-p1-lx-build2` (after the clang-format and the race fix) rc 0, no warning or error. `-L 'closures|common' -j2`: 36 of 37 pass, the one failure being the same `Closure_Flatten_Random` floors; `Closure_Layout` passes on Linux (SC-010: the layout holds on both platforms). `-L 'link|architecture' -j2`: **52 of 52 pass**.

### Architecture (T033)

`035-p1-arch` on macOS: 45 of 45 pass; `allowlist.txt`, `ribvector-core-residual.txt` and `metrics-baseline.txt` byte-identical to the Phase 0 copies; no `reference` file changed (`git status --short tests src/libshader/tests`).

### Swap

The macOS jobs ran with the swap free at 1.0 to 1.3 GiB, but the figure was 996 MB before `035-p1-race2` and 1004 MB afterwards (under the 1 GiB rule by 2 percent); the rest of the phase's macOS jobs waited for it to recover.

### Full run (macOS, after the formatting and the race fix)

`035-p1-all ctest -LE 'determinism-sweep|gi_determinism_long' -j4`: ctest wall 343.6 s (02:14 to 02:21), **1763 tests, 1762 passed, 1 failed**: `Closure_Flatten_Random` (the PAUSE above). 1763 is the Phase 0 baseline's 1745 plus the 18 closure tests. Neither of the two envshadow tests (#225) failed in this run, and nothing else is red. The swap was 1.24 GiB free at the start.

### Resolution of the `Closure_Flatten_Random` PAUSE (2026-11-24: the maintainer approved Option 2)

The floors are unchanged (`totalLobes > 3000`, `nonEmpty > 900`); the generator is richer: depth 3 to 6 (was 2 to 6), below depth 0 a node is a leaf with probability 1/6, a multiply with 1/3 and an add with 1/2 (was 1/3 each), a zero component 10% (was 12%) and a zero multiply 6% (was 10%). The checks themselves (the bit-for-bit comparisons of the flatten, the generic walk and the reference) are unchanged and run over the new population.

- macOS `-L 'closures|common'`: **37 of 37 pass**. `-L 'link|shading_system|libshader|shading_parity|architecture'`: **462 of 462 pass**; the three architecture files are byte-identical to the Phase 0 copies.
- Linux `-L 'closures|common' -j2`: **37 of 37 pass**; `-L 'link|architecture' -j2`: **52 of 52 pass**; the build is clean.
- **Mutation run (the floors still catch a degenerate generator)**, each reverted afterwards (`git diff --stat` empty): (1) the zero-component probability raised from 10% to 90%: `Closure_Flatten_Random` fails on both floors (`totalLobes > 3000`, `nonEmpty > 900`); (2) the tree depth set to 0 (single components): fails on both floors. The per-tree equality checks stayed green in both, as they should: only the population guards fail.
- Swap: the two mutation jobs ran with 844 and 860 MB free, **under the 1 GiB rule** (my check printed the figure but did not gate on it); every later macOS job waited for at least 1.09 GiB. The Linux runs are not swap-bound.

### Noted from the coordinator (master `71c5e440`, not in this branch yet)

#113 is merged on master: JIT `phong()` now shares its per-point term with the interpreter (`libshader::phongReflect` and `phongAccumulate` in `specularTerm.h`), a new `op_phong_batch_v2(result, Nf, sn, V, sv, size, ss, n, tags)` (the old `op_phong_batch` stays as a wrapper), `CShadingContext::callPhong(result, Nf, nfStep, V, vStep, size, sizeStep)` and changed `PHONGEXPR` macros in `shaderFunctions.h`; no image changed. It arrives at the next sync. Phase 3's lighting gate reaches `phong()` through `iterateLights` only, so it is unaffected; when a later phase touches the lighting builtins it builds on the shared term and adds no separate JIT path. The re-read of the funnels (`callPhong`'s line number and signature) is part of the next sync's re-read.

## Phase 2

### Swap gate (standing rule, coordinator 2026-11-24)

`specs/035-closure-tree-bsdf/tools/swapgate.sh` BLOCKS until at least 1024 MB of swap is free (it prints the figure each time it waits) and exits 1 after `SWAPGATE_MAX_WAIT` seconds; `tools/gated.sh <label> <command...>` runs a job behind it (`exec cpujob`) and exits 75 without starting the job if the gate gave up. Tested once: with an impossible figure it waited, gave up and the chained command did not run (`gated.sh` exit 75, `SHOULD-NOT-RUN` not printed); with a satisfiable one it printed `go` and the job ran under cpujob. Every macOS job of Phase 2 goes through `gated.sh`.

### Tests written first (T035 to T041) and their red evidence (T042)

Files (all new, under `tests/closures/`): `bsdf_checks.h` (the shared estimators), `test_bsdf_diffuse.cpp`, `test_bsdf_oren_nayar.cpp`, `test_bsdf_microfacet.cpp`, `test_bsdf_delta.cpp`, `test_bsdf_conventions.cpp`, `test_bsdf_determinism.cpp`; `CMakeLists.txt` registers them inside the red block (one ctest test per section, label `closures`, timeout 900 s). Red evidence (`035-p2-redcfg` with the option ON, then `035-p2-red cmake --build $WT/build --target <t> -j4` for each, behind the swap gate): all six executables fail to build on the first missing header, `bsdf.hpp` (`bsdf_checks.h:37` and `test_bsdf_conventions.cpp:24`); `bsdfMath.h` follows it. Logs `build/p2-red-<target>.log`.

| ctest tests | target |
|---|---|
| `Bsdf_diffuse_{Values,Reciprocity,Energy,PdfNorm,SamplePdf,ChiSquare,ReversePdf}` | `test_bsdf_diffuse` |
| `Bsdf_oren_nayar_{Sigma0,Reciprocity,Energy,PdfNorm,SamplePdf,ChiSquare,ReversePdf}` | `test_bsdf_oren_nayar` |
| `Bsdf_microfacet_{Math,Refusal,Reciprocity,Energy,PdfNorm,SamplePdf,ChiSquare,ReversePdf}`, `Bsdf_RoughnessFloor` | `test_bsdf_microfacet` |
| `Bsdf_reflection_Values`, `Bsdf_refraction_Values`, `Bsdf_Delta_Pair`, `Bsdf_Delta_Mixed` | `test_bsdf_delta` |
| `Bsdf_Horizon_Grid`, `Bsdf_Horizon_Invalid`, `Bsdf_NotScatterer`, `Bsdf_ImportanceFactor`, `Bsdf_Status_Table`, `Bsdf_Frame`, `Bsdf_Wrapper` | `test_bsdf_conventions` |
| `ClosureDet_BsdfRepeat` | `test_bsdf_determinism` |

### The tolerances (all in `bsdf_checks.h`; none is a free constant)

- `floatTol(a, b) = 512 * FLT_EPSILON * (|a| + |b|) + 1e-7`: two evaluations of one quantity in a different operand order (reciprocity, the reverse density, sample against evaluate/pdf, the closed-form values). 512 epsilon is 6.1e-5 relative; the absolute 1e-7 is for a value that is exactly zero on one side.
- `kRoundingFloor = 4 * FLT_EPSILON`, added to a `3 sigma` bound whose sigma can be near zero (a diffuse lobe's `f |cos| / pdf` is 1 for every sample).
- Statistical: `3 * sigma`, sigma = sd / sqrt(N) from the estimator's own sample variance and count; two estimators compared with `3 * sqrt(sigma1^2 + sigma2^2)`. Sample counts: 100000 for the sampled estimators, 200000 for the uniform-sphere estimators, 20000 for the pointwise checks, 400000 for the chi-square.
- Chi-square: a fixed 8 x 8 histogram (z uniform over [-1, 1], phi uniform: equal solid angle) against the density integrated per cell (a 48 x 48 midpoint rule per cell), one more cell for the failed/delta samples; cells with an expectation under 5 are left out; critical value of chi-square with (cells - 1) degrees of freedom at significance 0.001 by Wilson-Hilferty with z = 3.0902.
- Energy: the sampled albedo (IMPORTANCE mode) is at most `1 + 3 sigma`; for a lossless lobe (`diffuse`) it is `1 +- 3 sigma` (the white furnace); and it agrees with the independent uniform-sphere integral of `f |cos|`. Density normalisation: `integral(pdf) + P(delta) + P(failed) = 1` within `3 sigma` (the integral uniformly over the sphere, the two probabilities from the sampled fractions).
- Honest limit: the 3 sigma checks are statistical, so a test with many of them can fail by chance even when the code is right. The seeds are fixed (a keyed stream with test-local seeds), so a failure is reproducible; it will be reported with the observed value and sigma, and no bound will be widened.
- `microfacet` at alpha 0.05 skips the uniform-sphere estimators and the chi-square (a uniform estimator's sample variance is unreliable for a lobe that narrow, and a 48 x 48 midpoint rule cannot integrate its density per cell); it keeps reciprocity, the sampled-albedo bound, sample consistency and the reverse density.

### What the tests pin beyond the contract text (to confirm at TEST-APPROVAL)

1. `bsdfMath.h` (namespace `openrender::shading::bsdf`): `kMicrofacetDeltaAlpha` (1e-4f), `ggxD(alpha, cosThetaM)`, `beckmannD(alpha, cosThetaM)`, `fresnelDielectric(cosThetaI, eta)` (eta = n_t / n_i for a positive cosine, a negative cosine is the ray inside, eta <= 0 gives 1, total internal reflection gives 1).
2. `bsdf.hpp` (namespace `openrender::shading`): `Bsdf::prepare(tree, const OrShadingFrame &, CMemPage *&arena)` returning a `Bsdf` (default-constructed is invalid, `valid()`), `evaluate(q)`, `pdf(q)` returning `BsdfEvalResult { Spectrum f; float pdfForward, pdfReverse; uint32_t flags; OrStatus status; }`, `sample(q, u1, u2, uLobe)` returning `BsdfSampleResult` (the same plus `float wi[3]; float eta`).
3. The component operations work in the lobe's local frame; a component is one-sided unless it transmits. `wo` below the lobe's horizon gives nothing for `diffuse`, `oren_nayar`, `reflection` and a reflection-only `microfacet`; a transmissive lobe (`refraction`, `microfacet` refract 1 or 2) serves `wo` on either side (a negative `wo.z` is the ray inside, the relative index is inverted), which the adjoint reciprocity needs.
4. The radiance-mode factor is `(eta_wo / eta_wi)^2` with `eta_wo` the index of the medium `wo` is in (so 1/2.25 entering glass, 2.25 leaving it, the contract's 2.25 "glass to air"); importance mode is unscaled; reciprocity is the adjoint relation `f(wo, wi, m) == f(wi, wo, dual(m))`, plus plain symmetry for a reflection-only lobe.
5. A delta sample from a component carries `pdfForward == pdfReverse == 1`; the table replaces them by the lobe-choice probability. `OrBsdfSample::eta` is the relative index of the sampled transmission (wi's medium over wo's), 0 or 1 for a reflection is not asserted.
6. `refraction` is transmission only: past the critical angle its sample fails (`OR_BSDF_FAILED`); `reflection` with eta 0 has F = 1.
7. The table: `requested` is a mask of the lobe-kind flags (a lobe is requested when the mask holds one of its REFLECTION/TRANSMISSION bits and one of its DIFFUSE/GLOSSY/SPECULAR bits); `requested == 0` requests nothing and answers zero. `HAS_DELTA` is reported when a REQUESTED delta lobe exists. A lobe's selection probability is `mean(weight) * albedo(wo)` renormalised over the requested lobes; the tests fix it only where the albedo is known exactly (`diffuse` 1, a mirror with eta 0 is 1).
8. `prepare` refuses a hand-forged `microfacet` node with eta 0 and refract 1 or 2 with `OR_STATUS_BAD_ARGUMENT`; an allocator that returns null gives `OR_STATUS_OUT_OF_MEMORY`; the empty tree prepares and is a non-scatterer. `evaluate`, `pdf` and `sample` return `OR_STATUS_BAD_ARGUMENT` for a null pointer, a `structSize` below `sizeof(OrBsdfQuery)` or a NaN or infinite component of `wo` or `wi`, and `sample` also for a NaN or out-of-range `u1`, `u2`, `uLobe`; a larger `structSize` is accepted. A lobe whose own normal `N` is zero takes the shading frame's, and a non-unit `N` is normalised; `Ng` is read and changes no bit of the answer.
9. A `microfacet` below `kMicrofacetDeltaAlpha` samples bit for bit like `reflection` (refract 0) or `refraction` (refract 1) of the same eta; alpha exactly 1e-4 is not below it.

### Phase 2 status

T035 to T042 are done and staged. **T043 (TEST-APPROVAL) is the stop**: the implementation (T044 to T049, with `bsdf-sources.md`, the `refract == 2` verification and the Beckmann decision) waits for the maintainer's approval of the tests, the tolerance formulas and the API list above.

### Approval (T043), 2026-11-24

The maintainer approved the tests, the tolerance formulas and the ten pinned behaviours. Conditions: (1) the seeds are fixed, so a 3 sigma check that fails fails every time: report the observed value and sigma and change neither the check, the tolerance nor the seed; (2) `refract == 2` is verified only against OSL headers already on this machine, by a targeted local search; (3) the Beckmann D cos fallback is approved and recorded in `bsdf-sources.md`; (4) rules as before.

### Implementation (T044 to T049)

New: `src/libshader/closures/{bsdfMath.h,bsdfMath.cpp,bsdfComponents.h,bsdfComponents.cpp,bsdf.hpp,bsdf.cpp}` and `specs/035-closure-tree-bsdf/bsdf-sources.md`. Edited: `closureRegistry.{h,cpp}` (the five BSDF built-ins take their `ops` from `bsdfComponentOps()`), `src/libshader/shading/CMakeLists.txt` (three sources listed), `tests/closures/CMakeLists.txt` (the Phase 2 tests out of the red block). Nothing outside `src/libshader/closures`, one CMake list and the tests is touched; `stateVocabulary.h` is not.

- **T044** `bsdf-sources.md`: every function is written from the papers; nothing is adapted; no licence to check.
- **T045** `refract == 2`: **unverified**. The targeted local search found no `OSL/` directory, no `stdosl.h` and no `oslclosure.h` under `/opt/homebrew/include`, `/usr/local/include` or the vcpkg include directory, no `osl` formula under `/opt/homebrew/opt` or `Cellar`, and `docs/references/` holds only the RISpec and NSI documents. Implemented as the contract writes it (0 reflection, 1 transmission, 2 both, the reflection chosen with a clamp of the Fresnel term); for spec 049 to compare.
- **T046** the Beckmann fallback is taken (D cos sampling; GGX uses visible normals): approved, reported here.
- Two defects of mine found on the first green build, both fixed: an unused function (`-Werror`, GCC) and, more important, the directions were used as given (float, not exactly unit): near the critical angle a 1e-7 length error is magnified in the Fresnel term of the two orders of a reciprocity pair (differences of 3e-4 to 3e-2), so `makeSpace` now normalises both directions in double.

### First results (Linux, GCC 13 `-Werror`, `-j2`; the macOS side is in the next section)

`-L closures`: **53 tests, 48 pass, 5 fail**: `Bsdf_microfacet_{Reciprocity,PdfNorm,Energy,SamplePdf,ChiSquare}`. Every other Phase 2 test passes: the six checks of `diffuse` and `oren_nayar`, `Sigma0` (bit for bit), `Values`, `Math`, `Refusal`, `RoughnessFloor`, `ReversePdf` of `microfacet` for the whole grid, `Bsdf_reflection_Values`, `Bsdf_refraction_Values`, `Bsdf_Delta_*`, `Bsdf_Horizon_*`, `Bsdf_NotScatterer`, `Bsdf_ImportanceFactor`, `Bsdf_Status_Table`, `Bsdf_Frame`, `Bsdf_Wrapper`, `ClosureDet_BsdfRepeat` (hash identical on 1 and 4 threads, 100 repetitions, and for a BSDF each thread prepared for itself).

The five red tests are analysed line by line in `specs/035-closure-tree-bsdf/tools/proposed/phase2_test_defects.md`: no numerical check on a lobe the test can resolve fails. They are four defects of the approved tests (a non-vacuity guard that narrow or transmissive lobes cannot meet, a sample-count guard that a total-internal-reflection side cannot meet, an eta 1.0001 transmission lobe that is a spike the uniform-sphere estimators and the chi-square cannot see, and a chi-square whose failed-cell expectation is a quadrature error at a discontinuity). **PAUSE**: the proposals change approved tests, so nothing is changed; the five tests stay red until the maintainer decides. The seed rule is kept: no check, tolerance or seed was touched.

### Capability to fail (T050), run on Linux (the edit, the sync, the build and the run; each reverted: `git diff --stat` empty)

| # | Scratch edit | Result |
|---|---|---|
| 1 | `diffuse`'s `f` multiplied by `cos(theta_i)` | `Bsdf_diffuse_Values`, `_Reciprocity`, `_Energy` fail |
| 2 | the `(eta_wo / eta_wi)^2` radiance factor dropped (the numerator is 1 in both modes) | `Bsdf_ImportanceFactor` and `Bsdf_microfacet_Reciprocity` fail |
| 3 | the reverse density returns the forward density | `Bsdf_microfacet_ReversePdf` fails |
| 4 | a delta lobe's `evaluate` gets a finite density (0.5) | `Bsdf_reflection_Values` and `Bsdf_refraction_Values` fail. **`Bsdf_Delta_Pair` and `Bsdf_Delta_Mixed` do not**: the table skips a delta lobe's density by construction (`l.delta` is checked before the lobe's `evaluate` result is used), so the table tests cannot see it; the component tests do. Not a weak estimator, a property of the layering; reported |
| 5 | GGX `D` halved | `Bsdf_microfacet_Math`, `_PdfNorm`, `_Energy` and `_ChiSquare` fail |

### Linux results (T051, and the Linux half of T049 and T052)

GCC 13 `-Werror`, `-j2`, `tools/sync_linux.sh` (5237 files verified), full build rc 0 with no warning: `-L closures`: 53 tests in 18.7 s of ctest wall time, **48 pass, 5 fail** (the five `Bsdf_microfacet_*` of the PAUSE above); `-L 'link|common|architecture'`: **72 of 72 pass** (architecture 45 of 45). Per-test wall times on Linux (seconds): `Bsdf_microfacet_ChiSquare` 13.27, `_Energy` 6.64, `_PdfNorm` 5.20, `_SamplePdf` 1.83, `_Reciprocity` 0.58, `ClosureDet_BsdfRepeat` 0.47, `Bsdf_oren_nayar_ChiSquare` 0.39, `Bsdf_microfacet_ReversePdf` 0.36, every other under 0.2. The "long" tests are short: the tolerances are what cost, not time.

### macOS: PENDING (blocked on the swap gate, not skipped)

Free swap on this machine stayed between 700 and 960 MB for more than an hour (other sessions' jobs), under the 1 GiB gate, so `tools/gated.sh` correctly refused to start any macOS job: the first chain gave up at its 30 minute limit (`cfg rc=75`, the job not started, which is the behaviour tested), and a second chain (`SWAPGATE_MAX_WAIT=7200`: configure and build, `-L closures`, `-L 'link|common'`, `-L architecture`, each behind its own gate) was still waiting when this was written. Nothing of Phase 2 has run on macOS yet: the macOS build, the macOS test run and wall times, and the macOS `-L architecture` (T052) are pending. The build tree on macOS is still configured with the red option ON from T042 (T049 reconfigures it OFF).

### Phase 2: D1 to D4 applied (2026-11-25), Linux result

Approved by the maintainer (D1 and D4 as proposed, D2 narrowed, D3 option (a)); applied to `tests/closures/bsdf_checks.h` and `test_bsdf_microfacet.cpp` only
(`git diff` of the two files is in the report). Linux (GCC 13 `-Werror`, `-j2`, own tree): `-L closures` 51 of 53 pass (21.7 s real); the red ones are
`Bsdf_microfacet_SamplePdf` (D5, two alpha 0.05 rows with wo inside: 303 and 0 usable samples against the floor of 1000) and `Bsdf_microfacet_ChiSquare` (D6, five
rows over the critical value that a 128-sub-cell rule brings under it for four of them); both in `tools/proposed/phase2_test_defects.md`, awaiting a decision.
`-L 'link|common|architecture'` passes entirely (45 architecture tests, 7 link, 20 common). Reciprocity, Energy and PdfNorm are green (the sampled half of the
reciprocity pairs is non-vacuous; the eta 1.0001 refract 1 and 2 rows run the sampled estimator only). Dropped failed cells of the chi-square are logged
("failed cell dropped": for example ggx refract 1 alpha 0.9 wo.z +0.95, expected 216 < 4 x 1113).
**Finding (D3, for the record):** Beckmann refract 2 eta 1.0001 has an unbounded sampled energy: the D cos fallback weight of a near-index-matched
transmission is not bounded, so the `<= 1 + 3 sigma` bound is not meaningful there (hence option (a)); it is not a defect of the density, whose checks
(reciprocity, sample consistency, reverse density) pass.
**macOS: PENDING.** Not run: the tests change again after the D5/D6 decision, and one Mac chain after that is cheaper than two (swap, thermal); the Mac tree is
still configured with `OPENRENDER_035_RED_TESTS=ON` and is switched OFF in that run.

### Phase 2: D5 applied and D6 measured (2026-11-25)

D5 applied as approved: `Bsdf_microfacet_SamplePdf` skips the refract 1, alpha 0.05, eta 1.5, wo.z -0.60 rows (both distributions), the reason stated in the
test header. Linux `-L closures` -j2: 52 of 53, only `Bsdf_microfacet_ChiSquare` red (19.8 s). D6: the 0.0002 density deficit of the grazing GGX refract 1 rows is a
quadrature artifact of the hard cutoff of the GGX density at z = -wo.z / eta (finite jump; the density and the sampler agree in every band, 8e6 samples,
mean pdf ratio 1.000000, fine-grid integral 1.000000); the figures and the predicted-against-measured errors are in `tools/proposed/phase2_test_defects.md`.
No code change. The chi-square test is unchanged and still red, awaiting the maintainer's choice of (b). macOS remains PENDING.

### Phase 2: D6 resolved by (d) plus the pole cells (2026-11-25)

`integrateCells()` replaces the plain 48 x 48 midpoint rule of the chi-square's cell integrals: the two cells that touch a pole are integrated uniformly in theta (a smooth lobe is a function
of sqrt(1 -+ z) near a pole, O(h^1.5) in z) and a sub-cell holding a jump of the density is split at it; the jumps come from `microfacetEdges()`. Edges, derived and confirmed by a scan:
GGX transmission ends at wi.z = -wo.z / eta_rel (m.z = 0; found at -0.08000 for wo.z +0.12 and at 0.90000 for wo.z -0.60, eta 1.5); Beckmann has none (D = 0 at the horizon); the critical
cone is continuous; the reflection edge z = 0 is a cell boundary. Two rows were explained by the edge to the digit (errors at 48, 96, 192, 384 sub-cells: +0.004862, -0.001891, +0.001486,
-0.000203 against +0.00486, -0.00189, +0.001484, -0.000202); three by the pole cell (-73 to -80 counts in the cap, h^1.5). Chi-square before -> after: ggx r1 a0.9 +0.12 32.3 -> 4.8; ggx r2 a0.3 +0.95
108.6 -> 42.1; ggx r2 a0.9 +0.12 89.7 -> 30.3; beckmann r1 a0.3 +0.95 154.9 -> 10.0; beckmann r2 a0.3 +0.95 226.6 -> 36.0.
**Known limitation of the chi-square quadrature:** Beckmann's D cos fallback density (no factor wo.m) has a hard support boundary on the curve wo.m = 0, not a ring (its lowest z is
-wo.z / eta_rel - sqrt(1 - 1 / eta_rel^2) sqrt(1 - wo.z^2), -0.978831 for wo.z +0.5, eta 1.5: 0 below, 0.538 above at the extremum's azimuth). It is not split; the rows it touches pass.

### Phase 2 results with D1 to D6 applied (2026-11-25)

Linux (GCC 13 `-Werror`, `-j2`): `-L closures` 53 of 53 (13.9 s; ChiSquare 13.0 s, Energy 5.9 s, PdfNorm 3.5 s, SamplePdf 1.8 s, Reciprocity 0.8 s, ReversePdf 0.4 s); `-L 'link|common|architecture'` 72 of 72.
macOS (arm64, red option OFF, `-j4`): build clean; `-L 'link|common'` 27 of 27; `-L architecture` 45 of 45; `-L closures` **52 of 53: `Bsdf_microfacet_ChiSquare` fails on one row**, beckmann refract 2 eta 1.5 alpha 0.3 wo.z +0.50: chi-square 55.0 over 24 cells (df 23), critical 49.8.
The failure is a statistical tail event of a fixed seed, not a defect: the per-platform realisations differ (the same row reads 20.9 on Linux; all 68 rows differ between hosts, the densities and integrals are identical to 5 digits);
the 48 sub-cell expectation is accurate (192 sub-cells: largest cell difference 0.15 sigma, chi-square 51.35, still above); the statistic is calibrated: Linux, 600 other seeds, mean 23.02 (df 23), sd 6.43 (6.78 expected), 0 above 49.8
(0.6 expected); Mac, 61 other seeds, mean 22.31, 0 above 49.8, no cell with a systematic deviation (largest mean z x sqrt(K) -2.27 of 64). The Mac seed's tail is p ~ 5e-4; with 68 chi-square checks per platform at 0.001
the chance that some row exceeds is about 6.6% per platform. No seed or threshold was changed. Decision for the maintainer in `tools/proposed/phase2_test_defects.md` (D7).

### Phase 2: D7 resolved by (a), results on both hosts (2026-11-25)

`Bsdf_microfacet_ChiSquare` tests each row at the per-row level that keeps the family-wise level at 0.001 over the rows the test runs: n counted at run time (68 here), Sidak 1 - (1 - 0.001)^(1/n) = 1.471e-05
(z = 4.178; critical value for df 23: 63.2), printed in the log (`chi-square family: 68 rows, per-row level 1.471e-05`, and `critical ... at level ...` per row). A multiple-testing correction; no tolerance was widened for a defect.
Calibration of the statistic on other seeds (D7 above): Linux 600 seeds, mean 23.02, sd 6.43 (6.78 expected), 0 above 49.8; macOS 61 seeds, mean 22.31, 0 above 49.8. The macOS row that failed
(beckmann refract 2 eta 1.5 alpha 0.3 wo.z +0.50): 55.0 (51.35 with 192 sub-cell expectations) against 49.8 at 0.001; against 63.2 it passes. Only `bsdf_checks.h` (`Options::chiLevel`, `normalUpperPoint`,
`chiSquareCritical(df, level)`, the log line) and `test_bsdf_microfacet.cpp` (the count and the level in `main`; the options moved into one lambda) changed; no seed, sample count or other check.
Results with D1 to D7: Linux `-L closures` 53 of 53 (14.2 s; the other Linux labels were green at D6 and no source changed since); macOS (red option OFF, `-j4`): `-L closures` 53 of 53 (8.6 s, ChiSquare 8.5 s),
`-L 'link|common'` 27 of 27, `-L architecture` 45 of 45. T049 and T052 ticked; T053 (the checkpoint) is left for the maintainer.

## Phase 3

### The harness proof (T054, T055, R-5), 2026-11-25

`Closure_Harness_InProcessRender` (label `closures`; `tests/closures/test_harness.cpp`, `cldbl_backend.{h,cpp}`, `shaders/cl_probe_surface.sl`) is green on Linux (0.03 s) and macOS (0.37 s) with no production code of the phase.
Six `RiBegin`/`RiEnd` sessions in one process: three hiders (raytrace, zbuffer, reyes) x two formats (rslo, slo); each renders an 8 x 8 frame (`PixelSamples 1 1`, box filter, one thread, orthographic, a quad covering the frame, `Color` 0.25 0.5 0.75),
binds the wrapper through `Surface`, and reads the pixels back through `RiCustomDisplay` (no file). Asserted: every pixel equals the object's `Cs` (1e-3), `finish` once, the wrapper ran the real backend of the format asked for and no other, and the point count.
- **The point-count oracle (C2, SC-002)**: the wrapper's `execute` adds `currentShadingState->numRealVertices` of every call it receives (public, no production change). Measured, the same on both hosts and both formats: **raytrace 64 real points in 1 execute** (one primary ray per pixel; the derivative-helper vertices of the batch are not counted),
  **zbuffer 100 real points in 4 executes**, **reyes 100 real points in 4 executes** (the 8 x 8 frame's grids). It is the oracle: exact for raytrace (asserted == 64), the count of the `color` render itself for the grid hiders (the mode tests compare `both` against it).
- **How the wrapper finds the real instance**: its description holds the real description (loaded by `rsloBackend().load()` or `sloBackend().load()` from `<dir of the cldbl file>/<wrapped name>.<format>`; the cldbl file is one line, `<wrapped name> <rslo|slo>`); `instantiate()` makes the real instance with the real backend and wraps it in a `CShaderInstance` subclass
  that forwards `illuminate`, `setParameters`, `getParameter(Shape)`, `requiredParameters` and `prepare`, and whose `execute` goes through `CShadingContext::execute(this, locals)`, which reaches the wrapper's backend, which calls the real backend's `execute(context, *wrapped, locals)`; the real instance's `instanceKey` is set from the wrapper's before it runs.
  A format is chosen by the wrapper file (`cl_probe_rslo.cldbl`, `cl_probe_slo.cldbl`, written by CMake beside the compiled shaders), not by the precedence rule: `Option "shaderformat" "default"` is set to the same format and is irrelevant to a name no `.rslo`/`.slo` file has.
- **Deviation from the task text, to confirm**: the counting shadeop operand of `cl_probe_surface.sl` (and its module) is not written; the task itself prefers the `numRealVertices` oracle and says the shadeop counter is not the oracle unless the harness shows it exact. `cl_probe_surface.sl` is `Ci = Cs * Os; Oi = Os`.
- **`emitsClosures`**: not on the interface yet (T068); the wrapper will answer true for a `cldbl` instance of a capable kind (T069), and `executeClosures` calls the same delegated `execute`.
- **The gate probe** `cl_gate_probe.sl` (T055; compiled to both formats on both hosts) has 17 `output varying float` results (`r_ambient`, `r_diffuse`, `r_specular`, `r_phong`, `r_illum`, `r_illumcat`, `r_lightsource`, `r_occlusion`, `r_indirect`, `r_gather`; the not-gated `r_trace`, `r_transmission`, `r_visibility`, `r_environment`, `r_shadow`, `r_texture`, `r_specbrdf`); `illuminance(P, Nf, PI/2)` forms only (GitHub #126). How the test reads the outputs
  is decided with T059: the wrapper reads them from the real instance's locals after the delegated `execute` (test-side code, no production change).
- `Option "shade" "shaderformat"` is **not** an option (the scene-wide format is `Option "shaderformat" "default"`; `Attribute "shade" "shaderformat"` is the attribute): the harness found it, `Option "shade"` arrives with T064.

### Phase 3 tests and their red evidence (T057 to T060), 2026-11-25

Written, in the red block (`OPENRENDER_035_RED_TESTS`, each its own executable), all label `closuremode` (plus `closures`; the vocabulary one `state;closuremode`):
- `tests/unit/state/test_closuremode_vocabulary.cpp`: `State_ClosureModeVocabularyMirror`.
- `tests/closures/test_mode_state.cpp`: `ClosureMode_Precedence`, `_BadValue`, `_Scope` (nested blocks, `TransformBegin`, `FrameBegin/End`, `SolidBegin`, `ObjectBegin`, and the object instance taking the mode current at `ObjectInstance`, by two renders), `_RibRoundTrip`.
- `tests/closures/test_mode_step.cpp`: `ClosureMode_ColorIdentity_<hider>`, `ClosureMode_Both_<hider>` (rslo and slo), `ClosureMode_Closures_<hider>` (rslo and slo; with the cut-out opacity and `camerahitmode "primitive"`), `Closure_Interim_IR017_<hider>`, `Closure_Interim_IR018_<hider>`, `Closure_Interim_Precedence` (hiders raytrace, zbuffer, reyes). The task names the three-hider tests once (`ClosureMode_ColorIdentity`); there is one ctest per hider here. The IR-026 rows of the precedence are Phase 4's.
- `tests/closures/test_mode_gate.cpp`: `ClosureMode_Gate_interp`, `_jit`. `tests/closures/test_mode_det.cpp`: `ClosureDet_ThreadIdentity_<hider>`, `ClosureDet_HiderParity` (also the batch-shape check: camera hits and secondary hits of one input give one tree). `tests/closures/test_mode_arealight.cpp`: `ClosureMode_AreaLight_interp`, `_jit`, `ClosureMode_Photon`.
- `Closure_Network_Loopback_{color,both,closures}`: `test_depth_determinism --configs "||-P:2"` on a generated scene with shipped shaders (IR-017 in the two non-color modes).
- Support: `mode_support.h` (the consumer double with FNV hashes of the flattened trees, the session driver with the image, the wrapper counters and outputs, the captured warnings), shaders `cl_count_light.sl`, `cl_cutout.sl`, `cl_lit.sl` and the wrapper files (`cl_wrapper()` in the CMake: `cl_cap`, `cl_capconst`, `cl_gate`, `cl_gatecap`, `cl_gatecapconst`, `cl_cutcap`, `cl_lit`, `cl_litcap`, `cl_countlight`, `cl_arealight`).
  `cldbl_backend.h` now declares what the tests read (`resetOutputs`, `outputMean`, `lightRuns`, `lightPoints`, the kinds `diffuse` and `constdiffuse`); their bodies, `emitsClosures` and `executeClosures` are T069, after approval (a wrapper file may name a shipped shader, found on `SHADERS`).
Red evidence (Linux `-j2` and macOS `-j4`, red option ON, one target at a time): `test_closuremode_vocabulary` fails on `libshader/closures/closureMode.h` (no such file); `test_mode_state`, `test_mode_step`, `test_mode_gate`, `test_mode_det` and `test_mode_arealight` fail on `closureConsumer.h` (the first include of `mode_support.h`; the `closureFrame.h`, `closureMode.h`, `setDefaultClosureConsumer`, `closureMode` members and the `closuremode` token follow it); `Closure_Network_Loopback_*` run red:
`mode-color.rib (1): Parameter "closuremode" is not declared`, orender fails for the default configuration. `Closure_Harness_InProcessRender` stays green on both hosts. A syntax check of every test against stub headers of the missing APIs (scratch, removed) left only the missing-API errors (`setDefaultClosureConsumer`, `CAttributes::closureMode`, `COptions::closureMode`, `CShadingContext::closureFrame`).
No production file of the phase has been edited.

### Phase 3 implementation, first run (T062 to T074; 2026-11-25) and a pause for test changes

Implemented (staged): the token `RI_CLOSUREMODE` in the three places; `EClosureMode` (+ `closureModeName`/`closureModeFromName`) in `stateVocabulary.h`; `CAttributes::closureMode` and `COptions::closureMode` (default `Unset`,
copied by the memberwise copy, restored in `restore()`, answered by `find()` as the spelling, `"color"` when unset); `declare(RI_CLOSUREMODE, "string")` as the **very first declaration** (the table lists the newest first, so the declared-set reference
gains one line and no recorded variable moves); `Attribute "shade"` and a new `Option "shade"` branch in `rendererContext.cpp` (CODE_RANGE naming the value and the three spellings, previous value kept, CODE_BADTOKEN for an unknown token,
the inline-declaration retry); the ribOut cases (`Option "shade"`, `Attribute "shade" "closuremode"`: the declared-type fallback did not cover them, `Attribute "shade"` recursed on a bare token); `src/libshader/closures/{closureMode.h, closureFrame.{h,cpp},
closureConsumer.h, closureInterim.{h,cpp}, closureStep.{h,cpp}}`; `IShaderBackend::emitsClosures/executeClosures` (defaults: false, nothing; the `rslo`, `slo` and `CTstBackend` backends untouched); `CShadingState::lightingSuppressed`, the context's consumer/frame API and
`lightingGateClosed()`; the renderer core's `ClosureFrame` (made in `beginFrame()` before the hider loop, set on each context beside `setSamplingConfig`, deleted in `endFrame()`); the cldbl wrapper's `emitsClosures`/`executeClosures`, light counter and output oracle.
`kRendererServicesVersion` unchanged (2.6.1). Phase 3 tests moved out of the red block.

**`shade()` hunks** (`git diff -U0`, exactly): (1) the local `plan` declaration; (2) the inline fast test and the `planSurfaceStep` call before the `usedParameters` line; (3) the surface block (`runSurfaceStep` under `plan.buildTrees`, today's `groupExecute` otherwise);
(4) one `deliverClosures` call before `memEnd(threadMemory)`. Elsewhere in the file: the include and the static default consumer, `newState()` (`lightingSuppressed = 0`), the gate in `iterateLights` (the `saveCat` overload), the shared check in `callAmbient` and `prepareAmbient`
(`execute.cpp`: the `runAmbientLights()` macro), `gatherHeaderBegin`, and `gatherSample` (below); `irradianceLookup` top (irradianceCalls.cpp).

**The wrapper reads the real instance's outputs (condition (b)).** Under `color`, the 17 outputs of `cl_gate_probe` as read through the cldbl wrapper (means over the real points; the same values under the interpreter and the JIT): r_ambient 1, r_diffuse 0.572863, r_specular 0.0388116,
r_phong 0.0191686, r_illum 1, r_illumcat 1, r_lightsource 1, r_occlusion 0.978782, r_indirect 0.0155431, r_gather 4, r_trace 0.25, r_transmission 0, r_visibility 1, r_environment 0.25, r_shadow 0, r_texture 0.5, r_specbrdf 1: every gated output is non-zero under `color`, in both backends.
Under `closures` the ten gated ones read 0 (r_gather 1 before the fix below, 0 after) and the not-gated ones equal color's.

**Plan deviation, found by the gate test: `gather()` needs a fifth read of the flag.** `gather(..., 0)` still runs the body once (the loop control is `gatherEndAdvance`, after the first iteration), so setting the count to 0 in `gatherHeaderBegin` (the contract's funnel) left `r_gather` at 1 on both
backends. The gate now also reads the flag at the top of `CShadingContext::gatherSample` (shared by both backends): every active point takes the miss path with no ray traced (the body for no point, the else for all). `gatherHeaderBegin` and `gatherSample` are the two halves of one funnel.

**Test defects and the declared-set reference: proposals, nothing applied** (`tools/proposed/phase3_test_defects.md`). With the production code in place 16 of 33 `closuremode` tests failed; ten defects of the approved tests are listed there with a diff that, run in the Linux tree only, makes the 33 pass and
`-L 'closuremode|closures|state|common|architecture|shading_parity|shading_contract|arealight|link'` 677 of 678 (the one failure is `SceneInterface_Services`, whose pinned `declared_set.txt` gains `variables 125` and one line, `v 124 closuremode ...`; declared set 124 -> 125 variables, 29 globals unchanged).

### Phase 3 validation after the approvals (T075 to T080; 2026-11-25)

Applied (approved): the declared-set reference (`variables 124 -> 125`, `v 124 closuremode`; declared set 124 -> 125 variables, 29 globals unchanged); the gather() second half in `contracts/closuremode.md` and plan Q10; test fixes T1, T2, T4, T6, T7, T9, T10; T5 as `ClosureMode_Scope_SolidObjectRISpec` (WILL_FAIL while GitHub #235 stood; the mark is dropped, see "Sync merge with master" below);
T8 with the comment citing GitHub #236. **Open: T3** (the ray type of a rasterised grid): the contract says `RayType::None` (`shaderGlobals.h`, "from the batch's ShaderGlobals"), so `ClosureMode_Both_zbuffer`, `ClosureMode_Both_reyes` and `ClosureDet_HiderParity` stay red on both hosts until the maintainer decides (proposal in `tools/proposed/phase3_test_defects.md`).

**Results.** Linux (-j2): `-L 'closuremode|closures|state|common|architecture|shading_parity|shading_contract|arealight|link|scene_interface'` 679 of 682, the three T3 tests failing. macOS (-j4, behind the swap gate): the same labels, 679 of 682, the same three.
Everyday set `-LE 'determinism-sweep|gi_determinism_long'`: **macOS 1833 tests, 3 failed (the T3 tests), 1 skipped** (`LibShader_Format_rslo_lower_minor`), no #225 intermittent this time; **Linux 1834 tests, 4 failed**: `TextureTile_RenderByteIdentical` (#45, the Phase 0 baseline) and the three T3 tests. Phase 0 baselines: macOS 1743 of 1745 (two #225 intermittents), Linux 1744 of 1746 (#45 and the #122 intermittent): the new tests are the only addition.
`-L '^determinism$'`: **169 of 169 on macOS (148 s) and on Linux (405 s)**. `-L architecture`: 45 of 45 on both hosts; `tests/architecture/allowlist.txt`, `ribvector-core-residual.txt` and `metrics-baseline.txt` byte-identical to the Phase 0 copies (`cmp`).
The only reference file changed is `declared_set.txt` (approved). `kRendererServicesVersion` 2.6.1.

**Visible change: none without closuremode (FR-027, SC-001), macOS, `-t:1`, the Phase 0 oracle scenes re-rendered with the Phase 3 build:** `camera-dof.tif` `52beb3b9...405d`, `arealight-softshadow-quad-raytrace.tif` `545b3ed2...9b2b`, `arealight-softshadow-quad-raytrace-slo.tif` `ff8d98f2...757b`: all three **identical** to the Phase 0 hashes.
The `ClosureMode_ColorIdentity_*` tests (the explicit `color` equals the no-mention image, three hiders) also pass; `Closure_Network_Loopback_{color,both,closures}` (`-P:2` against local, byte for byte) pass.

**Warnings.** Linux: none in the Phase 3 build logs. macOS: 23 linker warnings, none from 035 (8 `ignoring duplicate libraries`, vcpkg LLVM; 15 `orender-wire-macos`: Swift package objects built for macOS 13.3, linked at 13.0).

**T077 mutation evidence (macOS, scratch edits, each reverted by `git checkout` of the staged file; `libRSLRuntime` + the test executables rebuilt, then the named tests):**

| Mutation | Result |
|---|---|
| (1) `~LightingGate()` does not clear the flag | `ClosureMode_Gate_interp`, `_jit` FAIL (`mixed.lightRuns > 0`: the color object shaded later is dark); `Closures_raytrace` passes |
| (2) `iterateLights` gate removed | `ClosureMode_Closures_raytrace` FAIL (`closures.lightRuns == 0`, `r_illum == 0`), `Gate_interp`, `Gate_jit` FAIL |
| (2b-1) `prepareAmbient` check removed (the JIT's `rsl_ambient_1v` path) | both Gate tests pass: an **unreachable site** (nothing emits or calls `rsl_ambient_1v`; see "Phase 3 follow-up" below), so no test can fail it |
| (2b-2) `callAmbient` check removed | `Gate_jit` FAIL (`closed.outputMean == 0` for `r_ambient`); `Gate_interp` passes |
| (2b-3) the interpreter's `runAmbientLights()` check removed | `Gate_interp` FAIL; `Gate_jit` passes |
| (3a) `gatherSample` gate removed | `Gate_interp` and `Gate_jit` FAIL (`r_gather` 1) |
| (3b) `gatherHeaderBegin` gate removed, `gatherSample` kept | both PASS: the header half is redundant given the sample half (kept as the contract's first half) |
| (4) trees for the derivative-helper vertices | survived the first `ClosureMode_Both_raytrace` (its probe had no derivative, so no helper vertex existed); **with T11 applied it FAILS** on macOS and Linux (`dBoth.trees == dColor.realPoints`) |
| (5) the object's `Os` written instead of the shader's `Oi` | `ClosureMode_Closures_raytrace` and `Closure_Interim_IR018_raytrace` FAIL (alpha 1 against 0.25) |

Survivors, resolved in the follow-up below: (2b-1) is unreachable (documented); (4) is killed by T11.

### Phase 3 follow-up (2026-11-25): T11 applied, and the prepareAmbient survivor

**T11 applied (approved).** `tests/closures/shaders/cl_deriv.sl` (a probe that uses `area(P)`, so a raytrace batch has derivative-offset vertices), the wrapper `cl_dercap`, the wrapper counter `cldbl::counters().allVertices` and, in `sectionBoth` for the raytrace hider, the scene-has-helpers check (`allVertices > realPoints`),
`dBoth.trees == dColor.realPoints` and `dBoth.image == dColor.image`. Unmutated production code: `ClosureMode_Both_raytrace` passes. **Mutation (4)** (builder and delivery over `numVertices`, the wrapper writing a tree per vertex): FAILS (`test_mode_step.cpp:91: check: dBoth.trees == dColor.realPoints`), on Linux and macOS (below).

**Mutation (2b-1), `prepareAmbient()`, is an unreachable site.** `prepareAmbient()` is called by exactly one function, `rsl_ambient_1v` (`rslBuiltins.cpp:240`), a C wrapper that nothing calls: `llvmEmitter.cpp` has exactly one emission site for `ambient()`, `op == "ambient"` -> `declareOp(mod, "op_ambient_batch", ...)` (`:2252` to `:2257`), whatever the operand shape (the same batch call serves a uniform context and a varying one, the comment at `:2969` says the batch width is required);
no source in the tree names `rsl_ambient_1v` or any `*_1v` entry outside `rslBuiltins.{h,cpp}`; none of the 22 shipped `.slo` that call `ambient()` references it (`grep -a rsl_ambient_1v` over `build/shaders/*.slo` and the test shaders: 0 files; `op_ambient_batch`: 22) and `cl_gate_probe.slo` calls `op_ambient_batch`; no test calls it. The gate check at that site stays (it keeps the three ambient sites identical, so a future caller cannot bypass the gate), but no compiled shader and no test can reach it: nothing to add a probe for.

### Phase 3: T3 applied (interim rule) and `-L closuremode` green (2026-11-25)

T3 is decided as an interim rule (option (a)); the source fix is spec 058 (GitHub #267). Applied: `ClosureMode_Both_{zbuffer,reyes}` expect `RayType::None` and `ClosureMode_Both_raytrace` expects `Camera`; `ClosureDet_HiderParity` the same for its front object (`None` for reyes, `Camera` for raytrace, `Secondary` for the occluder; the shape wording now says reyes gives grid + span, raytrace shades every hit as a span);
Q11 reworded in `plan.md` and `spec.md`; the rule added to `contracts/consumer-and-backend.md` ("a camera hit is `Camera`, or `None` on a grid batch"), marked interim, with the statement that spec 058 makes `classifyShading()` report `Camera` for every camera-visible batch in every camera hider and that consumers must not build on the `None` case.
`-L closuremode`: **Linux 34 of 34, macOS 34 of 34** (the swap gate passed; `ClosureMode_Scope_SolidObjectRISpec` was then the WILL_FAIL test, counted as a pass while GitHub #235 stood; the mark is dropped in the sync merge with master, see below). T075 ticked.

## Sync merge with master (2026-11-26, before Phase 4)

`git merge --no-ff --no-commit master` (master `f03a4b71`, 27 commits ahead, #229, #230 and #235 among them): no conflict (auto-merged `shading.cpp`, `shading.h`, `rendererContext.cpp`, `tests/CMakeLists.txt`). Same merge: `ClosureMode_Scope_SolidObjectRISpec` loses `WILL_FAIL` (GitHub #235 made `SolidEnd` and `ObjectEnd` restore the attributes): the CMake line goes, its comment becomes one line, the file header and the section comment of
`test_mode_state.cpp` keep only the RISpec rule; no assertion changed. Validation of the merged tree follows (filled in below).

### Sync merge validation (merge commit 7caf0a68, master f03a4b71)

Linux (own tree, `-j2`): build clean, 0 warnings; `-L 'closuremode|architecture'` **79 of 79** (`ClosureMode_Scope_SolidObjectRISpec` passes as a normal test); everyday set `-LE 'determinism-sweep|gi_determinism_long'` **2017 tests, 2 failed, 1 skipped**:
`TextureTile_RenderByteIdentical` (#45, the baseline) and `photonmap_ObjectDefinitionRelease`.
**Known failure that comes from master, not 035:** `photonmap_ObjectDefinitionRelease` (`tests/unit/photon_map`, from `777ff07b`/`c114e464`, the #64 photon-map release fix) fails 3 of 3 with `map freed at RiWorldEnd while an object definition's attributes still held it` (refCount 2). It passed in the last Linux everyday run before the merge. Cause (the coordinator's finding): master's #235 (`bf85686c`) makes `ObjectBegin`/`ObjectEnd` push and pop the attributes,
which invalidates the test's premise that the object definition shares the world primitive's attributes. Sent to the bug-fix session and to the maintainer as a #235 follow-up. Not changed here.

macOS (`-j4`, behind the swap gate): configure and build clean (22 linker warnings, the known `ignoring duplicate libraries` and `orender-wire-macos` ones); `-L 'closuremode|architecture'` **79 of 79** (`ClosureMode_Scope_SolidObjectRISpec` passes as a normal test); everyday set **2016 tests, 1 failed, 1 skipped**: the only failure is `photonmap_ObjectDefinitionRelease`, the same master failure as on Linux (it fails on both hosts);
no #45 on the Mac (a Linux-only failure). Accepted failures: #45 (Linux only) and `photonmap_ObjectDefinitionRelease`. The merged tree is green; Phase 4 starts.

## Phase 4: red evidence (T085)

Tests written before any implementation (T081 to T084), then `035-p4-redcfg cmake -S . -B build -DOPENRENDER_035_RED_TESTS=ON` (rc 0) and one gated `cmake --build build --target <t> -j4` per target (`build/p4-red-<target>.log`). All six fail to compile, rc 2, on the first missing piece:
- `test_bxdf_layout`: `'bxdfAbi.h' file not found` (`bxdf_layout_c17.c`).
- `test_bxdf_registry`, `test_bxdf_tree`: `'bxdfAbi.h' file not found`.
- `test_bxdf_statement`, `test_bxdf_shading`: `use of undeclared identifier 'RiBxdf'` and `'bxdfAbi.h' file not found`.
- `test_shading_parity_shader_parameter_bxdf_governed`: `'libshader/closures/bxdfInstance.h' file not found`.
Only the first missing header per translation unit is reported; the test bodies first compile when the headers exist. The build tree stays configured with the option ON until T096.

Open points of the Phase 4 tests, answered by the coordinator before T086: (a) `N = (0, 0, -1)` is right (I points from the eye, `(0, 0, 1)` for the orthographic camera); (c) `RiSurface("null")` is the no-shader path and a Bxdf bound before `WorldBegin` reaches the world. (b), by reading the code only (no run): `cldbl_backend.cpp` copies the real description's type and delegates `load`, `instantiate`, `initialise` and `execute` without looking at the shader type, and a displacement runs through `groupExecute` to the instance's backend `execute()`, so the wrapper handles a displacement shader; `captureOutputs` sums every `STORAGE_MUTABLEPARAMETER`, `TYPE_FLOAT`, `CONTAINER_VARYING` parameter whose name starts with `r_`, which `output varying float r_msg` is, so the capture works for message passing. Nothing in Phase 3 ran the wrapper on a displacement, so this is read, not run. One requirement on T094, not a test change: in the displace-only pass `shade()` prepares `currentAttributes->surface()` (the slot) when the displacement uses message passing, but in the full pass the IR-026 substitute replaces `surface` before `groupPrepare`, so `surface("x", v)` must still resolve through the slot Surface (RISpec 3.2 section 15.8) while a Bxdf governs.

## Phase 4: implementation (T087 to T094), written, not yet built

Written after the maintainer approved T086; nothing is built or run yet (T096). New files in `src/libshader/closures/`: `bxdfAbi.h`, `bxdfRegistry.{h,cpp}`, `bxdfInstance.{h,cpp}`, `builtinBxdfs.cpp` (added to `libRSLRuntime`'s `closure_sources`). Edited: `ri.h`/`ri.cpp` (`RiBxdf`/`RiBxdfV`, `VALID_BXDF_BLOCKS` = `VALID_ATTRIBUTE_BLOCKS & ~RENDERMAN_OBJECT_BLOCK`; no token constant: the grammar needs none), `riInterface.{h,cpp}` (a `RiBxdfV` that ignores the statement), `rib.l`/`rib.y` (`Bxdf`, the two rules of contracts/bxdf.md), `ribOut.{h,cpp}`, `rendererContext.{h,cpp}`, `python/prman.py`, `lua/prman.lua`, and in Shading `closureStep.{h,cpp}`, `closureInterim.{h,cpp}`, `shading.{h,cpp}`, `shaderParameters.cpp`. `stateVocabulary.h` is not touched and no variable is declared.

**The substitute assignment (T092).** `RiWorldBegin` (`rendererContext.cpp`): `worldDefaultSurface` is reset, then, only inside the `if (!currentAttributes->surface())` block that binds `defaultsurface`, set to `ShaderGroupRef::retain(cShader)`; the `CBxdfInstance` already in the current attributes' bxdf slot takes it (`earlier->substituteSurface = worldDefaultSurface`) BEFORE `attrs->setDefaultSurface(...)`, so `checkParameters()` stores the substitute's `usedParameters` (R1, B2). `RiBxdfV` sets `instance->substituteSurface = worldDefaultSurface` before `bindBxdf`, so a Bxdf bound with a pre-world Surface (no default bound) or before any `RiWorldBegin` has an empty substitute (the no-shader path), and never a user Surface. `RiBxdfV` never calls `takeInstanceKey()`.

**`shade()` hunks (T094), `git diff -U0 src/libshader/shading/shading.cpp`:** (1) the inline fast test gains the third term `currentAttributes->governingMaterial() == EMaterialBinding::Bxdf` and the call is `planSurfaceStep(*this, *currentAttributes, surface, g, usedParameters)` (the surface is now passed by reference: the plan replaces it); (2) the surface block is `if (plan.buildTrees) { runSurfaceStep } else if (surface != NULL) { groupExecute } else { no-shader }`, an equivalent restructuring for the Phase 3 rows that lets the Bxdf-with-consumer row run with `surface == NULL`. Nothing else in `shade()` changes.

**Message passing while a Bxdf governs (E12).** The message-passing builtins ask the services, which read `object->attributes->surface()` (the slot), not `shade()`'s local `surface`; the displace-only pass prepares `currentAttributes->surface()` too. Replacing the local variable with the substitute therefore leaves `surface("x", v)` reading the slot Surface, as RISpec 3.2 section 15.8 requires.

**Two defects found in my new tests while reading the code (not run), to be shown with T095:** `Bxdf_ColorSamples` passes `nRGB` laid out 2 x 3; `RiColorSamples` stores it as the 3 x n matrix `convertColor` indexes (`toRGB[i * n + j]`), so the expected colour needs `nRGB = {1, 0.5, 0, 0.5, 0, 0}`; and `Bxdf_Parameters` expects a lobe of weight 0 after clamping `Ks` to 0, but `closureFlatten` drops zero-weight lobes, so the expected list there is empty (a visible clamp is `roughness` 2 to alpha 1).

## Phase 4: build, tests and the two defects found (T096 to T100, macOS)

**Build.** Red option OFF, gated full build (`035-p4-build`, `-j4`): the first run (403 s) stopped on one test-side error, `CL_SOURCE_DIR` undeclared in `test_bxdf_registry.cpp`, fixed in CMake (a compile definition, the test body unchanged; approved as plumbing); the second run (`-- -k`) finished rc 0 with 2 compiler warnings in the whole log, none in a Phase 4 file.

**First label run** (`-L 'bxdf|closuremode|closures|shading_parity|state|common'`, 688 tests): 682 passed, 6 failed. Two were defects of mine in production code, found by the new tests and fixed:
1. **A flipped zero is `-0`.** `Closure_Bxdf_Consumer_{raytrace,zbuffer,reyes}` and `Closure_Bxdf_ConsumerHiders`: the delivered tree's hash differed from the hand-built one although `N = (0, 0, 64)`, `I = (0, 0, 1)` (scratch print, reverted) gave the right `N = (0, 0, -1)`: `faceforward` of `(0, 0, 64)` computed `sign * scale * 0 = -0.0` for the two zero components, and the trees are hashed byte for byte. `runBxdfStep` adds `+ 0.0f` (so a flipped zero is `+0`), which makes equal normals equal bytes on every hider.
2. **Message passing read the wrong locals.** `Closure_Bxdf_MessagePassing`: a displacement's `surface("x", v)` read 0.3794 (raytrace) and 0.46 (z-buffer, reyes) in the mean instead of 0.375, while the control with the Surface governing read exactly 0.375 (scratch run, reverted). The services copy a message from the locals prepared for ACCESSOR_SURFACE, and once IR-026 had replaced `surface` with the substitute those locals were the defaultsurface's, so the full pass read the wrong entries (the displace-only pass, which prepares the slot, read the right ones). Fix: `SurfaceStepPlan::messageSurface` is the slot Surface when a Bxdf governs and the stored `usedParameters` has `PARAMETER_MESSAGEPASSING`; `shade()` then prepares the slot Surface's locals in ACCESSOR_SURFACE (not run) and the substitute's own in `substituteLocals`, which is what the substitute executes with. The T094 requirement (E12) now holds in the full pass as well, in both formats and on the three hiders.

After the two fixes: 686 of 688. The two remaining failures are defects of mine in the test bodies, not in the code; their fixes are proposed and not applied (they are test-body changes), and were verified by a scratch run of the five tests with the fixes in place (5 of 5 passed), then reverted:
- `Closure_Bxdf_Consumer_zbuffer`: the z-buffer hider writes no opacity (`test_mode_step.cpp` skips it for the same reason), so the two alpha checks (`opacity 0.5`, primitive `Os 0.25`) are skipped for `zbuffer`.
- `ShadingParity_ShaderParameterBxdfGoverned`: RiWorldBegin binds `defaultlight`, whose parameters belong to every answer from then on (the oracle of `shader_parameter_fixtures.h` counts it); the five expectations after `RiWorldBegin` add `lightBits`.

**T097, everyday set** (`-LE 'determinism-sweep|gi_determinism_long'`): 2054 tests; failed `photonmap_ObjectDefinitionRelease` (the known master failure), and the two tests above until their fixes are approved; `LibShader_Format_rslo_lower_minor` skipped, as at Phase 0. `Shaders_BytesUnchanged` and `ShaderCompilerImmutability` pass; `git status` shows no reference, `shaders.sha256` or allowlist file touched. **Determinism set** (`-L '^determinism$' -j2`): 169 of 169.

**T100, `-L architecture`:** 45 of 45; `ribvector-core-residual.txt` and the allowlist unchanged.

**T098, capability to fail** (scratch edits, each reverted by restoring the three files from copies, then a full rebuild; `git diff` empty afterwards):

| # | Scratch edit | Test that went red |
|---|---|---|
| 1 | `takeInstanceKey()` in `RiBxdfV` | `Bxdf_NoKey` |
| 2 | the substitute is the user's Surface | `Closure_Interim_IR026_ByteIdentity_{raytrace,zbuffer,reyes}`, `Closure_Interim_IR026_PreWorld_{raytrace,zbuffer,reyes}` (6 of 6) |
| 3 | `pendingInstanceKey` left set on an unknown name | `Bxdf_Refusals` |
| 4 | `VALID_BXDF_BLOCKS = VALID_ATTRIBUTE_BLOCKS` (Bxdf accepted in an object block) | `Bxdf_ObjectBlock` |
| 5 | GGX alpha squared | `Bxdf_Tree_plastic`, `_conductor`, `_dielectric` (3 of 5; diffuse and NoOsPremultiply use no GGX) |

## Phase 4: Linux (T099) and checkpoint (T101)

Ubuntu 24.04, GCC 13 with `-Werror`, own tree `openrender-035`, remote `cpujob` at `-j2`, after `sync_linux.sh` (5325 files verified). Configure with the red option OFF and build: rc 0, no warning-as-error. `-L 'bxdf|closuremode|closures|shading_parity|state|architecture'` (714 tests): 712 passed; the two failures are the two test-body defects above (`Closure_Bxdf_Consumer_zbuffer`, `ShadingParity_ShaderParameterBxdfGoverned`), the same two as on macOS. Everyday set: 2055 tests; failed `TextureTile_RenderByteIdentical` (#45, Linux only), `photonmap_ObjectDefinitionRelease` (master) and the same two; `LibShader_Format_rslo_lower_minor` skipped. Determinism set: 169 of 169.

**Status at the checkpoint.** Phase 4 production code and tests are built and run on both hosts. Open, awaiting the maintainer: the two test-body fixes (diffs in the scratchpad: `fix/governed.diff`, `fix/shading.diff`), after which the two tests pass (scratch run: 5 of 5). Everything else is green except the known master failure and, on Linux, #45. Changes against plan.md and the contracts: (1) `planSurfaceStep` takes the attributes and a reference to the surface (the plan replaces it); (2) `SurfaceStepPlan::messageSurface` and `shade()`'s `substituteLocals` (message passing while a Bxdf governs, E12); (3) `CShadingContext::defaultClosureConsumer()` (a static getter, read by `usedShaderParameters`); (4) `+ 0.0f` on the normal handed to a Bxdf so a flipped zero is `+0`; (5) the `ColorSamples` test matrix reads the same in both layouts (#269).

## Phase 4: final run after the two approved test-body fixes (T101)

The maintainer approved both fixes (`Closure_Bxdf_Consumer_zbuffer` skips the alpha checks for the z-buffer hider, which writes no opacity; `ShadingParity_ShaderParameterBxdfGoverned` counts `defaultlight`'s bits after `RiWorldBegin`). Applied and staged; `test_bxdf_shading` and the governed parity target rebuilt (macOS gated full build rc 0; Linux incremental build rc 0 after `sync_linux.sh`, 5325 files verified).
- macOS: `-L 'bxdf|closuremode|closures|shading_parity|state|common'` 688 of 688; `-L architecture` 45 of 45.
- Linux (Ubuntu 24.04, GCC 13 `-Werror`, `-j2`): the same label run 688 of 688; `-L architecture` 45 of 45.
- The everyday set (macOS 2054 tests, Linux 2055) was run before the two fixes with only the two test defects, `photonmap_ObjectDefinitionRelease` (master) and, on Linux, #45 failing; the two fixes touch only those two test files, so nothing else moved. Determinism set 169 of 169 on both hosts.
T081 to T101 are ticked in `tasks.md`; the maintainer's commit is the step that follows and is not recorded here.

## Phase 5: the sweep scenes and the red evidence for Closure_DocPaths (T102 to T105)

**Scenes (T103).** `specs/035-closure-tree-bsdf/tools/gen_scenes.py` writes the 24 scenes `examples/rib/tests/closures-{modes,bxdf-interim,arealight,instancing}-{reyes,zbuffer,raytrace}[-slo].rib`; `--check` regenerates in memory and compares (rc 0: a second run gives the same bytes). The shader format of an `-slo` scene is `Option "shaderformat" "default" ["slo"]` (the first draft used the token `"string default"`, which the option does not accept: every `-slo` entry failed in the measurement and the generator was corrected before any entry was registered).

**Measurement before the proposal (T104).** All 24 were run the way `add_determinism_sweep` runs them (the same `test_depth_determinism` command line, the sweep's environment, the default configurations `-t:1||-t:2|-t:4|-P:2`, the sobol line, `--float`), through one gated cpujob job: 24 of 24 pass, the `-P:2` loopback included, so none carries `# no -P:2`. The warnings show in every configuration, once per name per frame: IR-017 per (shader, mode) for `closures-modes` and `closures-arealight`, IR-026 per Bxdf name per frame for `closures-bxdf-interim` (four names in frame 1, two in frame 2) and `closures-instancing`.

**Registration (T104, approved).** The 24 `add_determinism_sweep(... FLOAT)` lines are between `# BEGIN spec 035` and `# END spec 035` in `tests/visual/determinism-scenes.cmake`; the two `closures-bxdf-interim-*` entries per hider and format list their two outputs (`<name>.tif`, `<name>-b.tif`). Existing entries and references are untouched. `ctest -N -L '^determinism-sweep$'` lists **785** tests (761 before, plus 24; 24 of them named `DeterminismSweep_closures-*`).

**`Closure_DocPaths` (T102, T105).** `tests/closures/check_doc_paths.cmake`, registered in the red block, label `closures`. With the red option ON it fails, as it must before the documents exist: `DEVNOTES_DETAILS/CLOSURES.md` missing, `RIB_GUIDE.md` names neither `Bxdf` nor `closuremode`, `BINDINGS_GUIDE.md` does not name `Bxdf`, the site page `docs/site/content/manual/reference/closures.md` is missing and `docs/site/content/manual/_index.md` does not link it (`build/p5-docred.log`).

## Phase 5: the documents (T107 to T109) and the CLAUDE.md draft (T110)

`Closure_DocPaths` (T106 approved, with the `_index.md` check matching the link target `reference/closures` instead of the bare word): re-run red after the change, it still fails for the five reasons (the guide and the site page missing, `RIB_GUIDE.md` naming neither `Bxdf` nor `closuremode`, `BINDINGS_GUIDE.md` not naming `Bxdf`, `_index.md` not linking `reference/closures`); after the documents, the script run by hand reports "closure ABI 1.0.0, Bxdf interface 1.0.0 ... agree". Written: `DEVNOTES_DETAILS/CLOSURES.md` (the tree and its layout table, the registry and the OSL names, the ten BSDF conventions, the three modes and four layers, the surface-step table, IR-017/018/026 and who replaces each, the lighting gate's four funnels, the consumer and its lifetime rule, the Bxdf table and built-ins, the two versions, the decisions implemented and what is left to 036, 041, 042, 049, 051 and 052; tree weights not premultiplied by `Os` and `surface("x", v)` reading the slot Surface while a Bxdf governs are stated), the `Bxdf` and `closuremode` section of `RIB_GUIDE.md`, the `Bxdf` helper section of `BINDINGS_GUIDE.md`, a status row in `DEVNOTES.md`, the 035 row of `RENDER_ARCHITECTURE_REFERENCES.md` section 13, and the Hugo page `docs/site/content/manual/reference/closures.md` with its link in `docs/site/content/manual/_index.md`. `specs/035-closure-tree-bsdf/claude-md-draft.md` is written for the maintainer; `CLAUDE.md` is not edited.

## Phase 5: closing validation on the final, unsynced tree (T117 to T125)

The tree is the branch at Phase 4's commit plus Phase 5's staged files; the second master sync (T112 to T116) has not been done and waits for the coordinator. Master has moved (#268, #269, #236 and others): nothing of that is in this tree.

**macOS (swap-gated cpujob, `-j4`; `-j2` for the sweep).**
- T117 final build (red option OFF): rc 0 (263 s incremental). `git diff f03a4b71 --stat -- shaders src/libshader/compiler tests/shaders/references src/libshader/tests/reference` is empty (no `.sl`, no compiler file, no shader reference changed); `Shaders_BytesUnchanged` and `ShaderCompilerImmutability` pass (in the everyday set).
- T118 everyday set (`-LE 'determinism-sweep|gi_determinism_long'`): 2055 tests; the only failure is `photonmap_ObjectDefinitionRelease` (the known master failure, #268 fixes it on master); `LibShader_Format_rslo_lower_minor` skipped as at Phase 0.
- T119 determinism: the sweep `-L '^determinism-sweep$' -j2` **785 of 785** (the 761 old entries unchanged and the 24 new ones), 581 s of test time; `-L '^determinism$' -j2` 169 of 169; `-L '^gi_determinism$' -j4` 22 of 22.
- T120 `-L 'closures|closuremode|bxdf' -j4`: **126 of 126**. The lists (`ctest -N -L`): `closures` 125, `closuremode` 59, `bxdf` 37. In-process determinism checks of SC-006 that are registered tests: `ClosureDet_ThreadIdentity_{reyes,zbuffer,raytrace}`, `ClosureDet_HiderParity` and `ClosureDet_BsdfRepeat` (5 of 5 pass). **There is no registered test named `ClosureDet_BatchShape`**: the batch-shape comparison (the same input through a grid and a span batch, FR-022) is a part of `ClosureDet_HiderParity` (`tests/closures/test_mode_det.cpp`), so the "six checks" of research R22 are five test names today; reported for the maintainer's decision.
- T121 byte-identity oracle (`-t:1`, the Phase 0 command, `build/p5-oracle/`): `camera-dof.tif` `52beb3b9419db88ee01a526a56ca111b94f743fb9fad5f745b1daec5840e405d`, `arealight-softshadow-quad-raytrace.tif` `545b3ed2d559b6201d83adfc4834786c7d3a17700822bf11c4d2b55d564f9b2b`, `arealight-softshadow-quad-raytrace-slo.tif` `ff8d98f29c6c9048877a17d9833f9721d5a49cfd9b844871835d70e5dd48757b`: all three `cmake -E compare_files` identical to `build/p0-baselines/oracle/`.
- T122 by-hand render (`closures-bxdf-interim-raytrace.rib`, `-t:1`): six IR-026 warnings, one per Bxdf name per frame (frame 1: `diffuse`, `plastic`, `conductor`, `dielectric`; frame 2: `conductor`, `dielectric`); `closures-modes-raytrace.rib`: one IR-017 warning per (shader, mode) (`plastic` under `both`, `matte` under `closures`). The renders equal the no-material objects in `Closure_Interim_IR026_ByteIdentity_*`.
- T123 `-L architecture`: 45 of 45; the three baseline files (`allowlist.txt`, `ribvector-core-residual.txt`, `metrics-baseline.txt`) are byte-identical to `build/p0-baselines/`; no `DOMAIN_BOUNDARIES.md` entry added.

**Linux** (Ubuntu 24.04, GCC 13 `-Werror`, own tree, `-j2`, after `sync_linux.sh`, 5354 files verified). Build rc 0. Everyday set: 2056 tests; failed `TextureTile_RenderByteIdentical` (#45, the starting tree's), `photonmap_ObjectDefinitionRelease` (master), and `Visual_sphere-texture3d-bake3d-reyes` (the intermittent texture3d `bake3d` segmentation fault, GitHub #122, seen on this host in spec 048 as well); `-L architecture` 45 of 45; `-L 'closures|closuremode|bxdf'` 126 of 126 (`Closure_Layout` and the other layout checks pass on both platforms, SC-010); `-L '^determinism$'` 169 of 169; the sweep 784 of 785: `DeterminismSweep_sphere-texture3d-bake3d-reyes` crashed in its default configuration (the same #122 fault); both `sphere-texture3d-bake3d-reyes` tests, re-run alone four times with `--repeat until-fail:5`, passed. Nothing else differs from the starting tree's failing set; none of the 24 new scenes differs from itself on any host.

**T125, release checklist for the maintainer.**
- Commands: the build `cmake --build build --config Release -j4` (macOS) and `-j2` (Linux); `ctest -LE 'determinism-sweep|gi_determinism_long'`; `-L '^determinism-sweep$' -j2`; `-L '^determinism$' -j2`; `-L '^gi_determinism$' -j4`; `-L 'closures|closuremode|bxdf'`; `-L architecture`; the three oracle renders; the by-hand render. All through the swap-gated `cpujob`.
- Counts against the starting tree: everyday 2016 at the first sync, 2055 now on macOS (+39: the Phase 4 tests, the 24 are in the sweep not here, `Closure_DocPaths`); sweep 761 to 785 (+24); determinism set 169 to 169; architecture 45 to 45.
- **Zero widened thresholds, zero re-captured references**; one reference gained a line and was approved in Phase 3: `tests/unit/scene_interface/reference/declared_set.txt` (`variables 124` to `125`, `closuremode` added: a new pre-declared variable, not a re-capture). No `.sl`, compiler or shader reference changed; `shaders.sha256` untouched; the allowlist and the residual list unchanged.
- Interface versions: closure ABI 1.0.0, Bxdf interface 1.0.0 (new), `kRendererServicesVersion` unchanged (2.6.1).
- Final lists: `ctest -N -L closures` 125, `closuremode` 59, `bxdf` 37, `determinism-sweep` 785.
- Deletions: none.
- First sync: 7caf0a68 (master f03a4b71). Second sync: not done; open for the coordinator (T112 to T116): put `r_shadow` back into the gate equality check once #236's merge (36988670) is in the tree and show it on both backends; check that no closures scene sets `ColorSamples` to a count other than 3 and then uses a 3-float `Color` or `Opacity` (#269); re-count the three label lists; update the bug list of the Recent Changes draft; renumber the gotcha to 49 (already in the draft).
- Findings for decision: the missing `ClosureDet_BatchShape` name (above); the Linux `bake3d` intermittence is #122, unrelated.

## Phase 5: the wording of the in-process determinism checks (maintainer decision, option 2)

The five registered `ClosureDet_*` tests stay (`ClosureDet_ThreadIdentity_{reyes,zbuffer,raytrace}`, `ClosureDet_HiderParity`, `ClosureDet_BsdfRepeat`) and `ClosureDet_BatchShape` is **not** registered: the grid-versus-span comparison of FR-022 is a section of `ClosureDet_HiderParity`. Corrected to say so: `research.md` R22 (the test list and the SC-006 sentence: five in-process checks, planned as six), `tasks.md` (the T120 text, and the Phase 3 table row that named `ClosureDet_BatchShape`) and `DEVNOTES_DETAILS/CLOSURES.md` (the closing paragraph). Corrected afterwards on the coordinator's instruction, with the same wording: `spec.md` SC-006 and the Q15 row of its clarification table, and `plan.md`'s Q15 row. Left as it is, for the maintainer: a comment in `tests/closures/test_mode_det.cpp` that names `ClosureDet_BatchShape` (an approved test file). The T125 checklist's finding on `ClosureDet_BatchShape` is closed by this decision.

## Second master sync (T112 to T116)

**The merge** (`git merge --no-ff --no-commit master`, committed by the maintainer as 012f97f6). Master 36988670, 6 commits ahead of the branch: #268 (the `.rslo` loader no longer writes a call-site operand over the first constants of a shader), #269 (ColorSamples reads `nRGB` as n x 3; RIB `Color` and `Opacity` carry n floats, a wrong count is `CODE_MISSINGDATA`), #236 (a traced `shadow()` outside a light and outside `illuminance` answers a defined 0; `shade()` and the light loops zero `L`, `Cl` and `Ol`) and master's `CLAUDE.md` gotcha 48. **No conflict**: `shading.cpp`, `shading.h`, `ribOut.{h,cpp}`, `ri.cpp`, `rib.y`, `options.cpp` and `DEVNOTES.md` merged automatically; no file was deleted.

**What was read before building (T113).** `shade()`: the three 035 hunks (the inline fast test with the plan call, the surface block through `runSurfaceStep`, `deliverClosures` before `memEnd`) and the `substituteLocals` preparation are intact and enclose the same statements; master's new zeroing of `L`, `Cl` and `Ol` sits after `complete()` and before `memBegin(threadMemory)`, ahead of all of them, and `clearLightGlobals` is called from `iterateLights`, `iterateBoundAreaLight`, `callAmbient` and `prepareAmbient` at the entry of each light run, beside (not inside) the four gate funnels, which are unchanged. `rib.y` still has the two `RIB_BXDF` rules and the token; `ribOut` and `ri.cpp` keep `RiBxdf`/`RiBxdfV` and `VALID_BXDF_BLOCKS`; `usedShaderParameters` untouched. `kRendererServicesVersion` is unchanged by the three fixes.

**Merged-tree validation (T114, T115).**
- macOS (gated, `-j4`; `-j2` for the sweep): configure and build rc 0 (399 s); everyday set **2100 of 2100 passed** (`photonmap_ObjectDefinitionRelease` passes now; `LibShader_Format_rslo_lower_minor` skipped); `-L 'closures|closuremode|bxdf|state|common|shading_parity|shading_contract|arealight'` 862 of 862; `-L '^determinism$'` 169 of 169; `-L architecture` 45 of 45; the sweep **785 of 785** (638 s).
- Linux (Ubuntu 24.04, GCC 13 `-Werror`, `-j2`, 5379 files synced): build rc 0; everyday 2101 tests, the only failure is `TextureTile_RenderByteIdentical` (#45, the starting tree's); `-L 'closures|closuremode|bxdf|architecture'` 171 of 171; `-L '^determinism$'` 169 of 169; the sweep **785 of 785** (1543 s).

**Follow-ups on top of the merge commit (T116), staged.**
- **r_shadow back in the gate equality** (`tests/closures/test_mode_gate.cpp`): it joins the not-gated list (`closed == color`), and the range check it replaced is gone; the comment now says why (#236). `ClosureMode_Gate_interp` and `ClosureMode_Gate_jit` pass on macOS and on Linux, so the equality holds on both backends.
- **The #269 check**: no closures scene or test sets `ColorSamples` to a count other than 3 and then uses a 3-float `Color` or `Opacity`. The 24 sweep scenes never call it; the only user is `Bxdf_ColorSamples` (a 2-sample session with a `color` Bxdf parameter and no `Color` or `Opacity` statement, and a matrix that reads the same in either layout); it passes on the merged tree on both hosts.
- **`ClosureDet_BatchShape`** removed from the last two places: `spec.md` FR-022 and the comment in `tests/closures/test_mode_det.cpp` (a comment only: now "the grid-versus-span section of ClosureDet_HiderParity").
- **`claude-md-draft.md`**: the Recent Changes bug list now says #235, #236 and #269 are on master and #267 is open; the three label counts re-counted on the merged tree: `closures` 125, `closuremode` 59, `bxdf` 37 (unchanged), sweep 785.
- After the follow-ups: `-L 'closures|closuremode|bxdf'` 126 of 126 on macOS; `-L 'closures|closuremode|bxdf|architecture'` 171 of 171 on Linux.
