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
