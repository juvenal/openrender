# Validation record: Scene Interface Untangle (spec 038)

One section per phase, appended at each checkpoint. Commands run through `~/.claude/bin/cpujob <label> <command>`; `$WT` is `/Volumes/Projects/Development/CLI/openrender-worktrees/038-scene-interface-untangle`. macOS is Apple Silicon (Clang, vcpkg-vendored dependencies and LLVM); Linux is `juvenal@ubuntu-2404-desktop.v2-labs.io`, tree `~/Projects/Development/CLI/openrender-038`, GCC 13 `-Werror`, `-j2`.

## 0. Phase 0: merge, configure, re-measure (2026-11-02)

### 0.1 Base and merge (T001 to T003)

- `git merge-base --is-ancestor 01501f10 HEAD` exits 0 (HEAD `3b8d8478`, "Merge master (spec 026 and the count fixes)"). The merge's only conflict was `.specify/feature.json`, which stays on 038.
- `grep -c '^28\. ' CLAUDE.md` prints 1; `CLAUDE.md` line 283 says 419 scenes for the determinism sweep; the DB-015 header in `tests/architecture/allowlist.txt:297` says `(10)`; the DB-011 evidence in `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` says 558 (lines 312 and 329). T001 to T003 are ticked.
- `CLAUDE.md` copies: the worktree's and the main checkout's are byte-identical (same `git log -1 --format=%ct -- CLAUDE.md`, 1793635596); the worktree copy was followed.

### 0.2 Decision D4-a applied to the documents (before T004)

GitHub #136 (`CRenderer::temporaryPath` is never cleared at `RiEnd`, so `locateFile`'s last resort searches a deleted directory after a renderer session) is cited as the core defect; 038 does not fix it and no task touches `endRenderer`. Edited: spec.md (Clarifications, FR-005, SC-004, Assumptions), research.md (M4, R3, section D), plan.md (decision table, risk table), contracts/scene-services.md (standard file search), contracts/checks.md (`SceneInterface_Services` row), tasks.md (T036 gains the D4-a case, T132 gains NEWS line (e)).

### 0.3 Disk, configure, build, architecture (T004 to T007)

- T004: `/Volumes/Projects` 9.0 GB free at the start (8.6 GB after the build); Linux `$HOME` 11 GB free; both `cpujob --status` free.
- T005: `cpujob 038-p0-cfg cmake -B $WT/build -S $WT` exit 0; `build/CMakeCache.txt` `CMAKE_HOME_DIRECTORY` is the worktree path. JIT on, `-Werror`.
- T006: `cpujob 038-p0-build cmake --build $WT/build --config Release -j4` finished with no error; the log (`$WT/build/p0-build.log`) holds only `ld: warning: ignoring duplicate libraries` lines (vcpkg archives, pre-existing), no compiler warning.
- T007: `cpujob 038-p0-arch ctest --test-dir $WT/build -L architecture --output-on-failure -V` (log `$WT/build/p0-arch.log`): 21/21 passed. `Architecture_DomainBoundaries`: `archcheck: platform=macos objects=295 sources=200 edges=2544 (include=1639 link=905)`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`.

### 0.4 Re-measured counts (T008)

| # | Quantity | Documents quote | Measured on the merged tree | Result |
|---|---|---|---|---|
| a | DB-015 allowlist lines | 10 lines at `allowlist.txt:298-307`, header `:297` | header `:297` ("(10)"), lines `:298-307`: 4 `include` of the generated parsers (`ifexpr_generated.cpp`, `rib_generated.cpp` to `renderer.h`/`rendererContext.h`), 2 `include` of `riHooks.h` (`ribOut.cpp`, `ri.cpp`), 2 `link` of `rib_generated.cpp` (`locateFile`, `retrieveVariable`), 2 `link` of `ri.cpp` (`initDeclarations`, `shutdownDeclarations`) | equal |
| b | Scene Interface metric and TOTAL | 6/6 and 612/94 | `metrics-baseline.txt:54` `SceneInterface 6 6`, `:57` `TOTAL 612 94`; archcheck: `SceneInterface refs 6 (baseline 6) distinct 6 (baseline 6)`, `TOTAL refs 612 (baseline 612) distinct 94 (baseline 94)`; Geometry 48/15, Hiders 558/82, others 0/0 | equal |
| c | CONSUMER section | 66 edges (28 `src/libshader/tests/*`, 38 tools); registry says 36 | 66 edges: 28 `src/libshader/tests/*` and 38 from `oshader.cpp` 3, `sloinfo.cpp` 3, `precomp.cpp` 1, `orender.cpp` 1, `otexmake.cpp` 1, `previewContext.cpp` 9, `ribGeometryContext.cpp` 10, `ribGeometryContext.h` 5, `dataLoad.cpp` 5. By target domain, tools: Geometry 15, Graphics State 4, Hiders 3, Scene Interface 8, Shading 6, Texturing 2. | equal to M14. What DB-024's "36" counts: the 38 tool edges minus the two `ri.h` edges of `orender.cpp` and `otexmake.cpp` (M14's guess is confirmed arithmetically: 3+3+1+15+4+5+4+1 = 36, i.e. oshader, sloinfo, precomp, the preview's 15 Geometry + 4 Graphics State + 5 Scene Interface, `dataLoad.cpp`'s 2 Hiders + 2 Texturing + 1 Scene Interface). The registry text says "3 Graphics State headers and 4 Scene Interface headers" for `libribpreview`; the check lists 4 and 5 (the 36 total is unaffected). |
| d | `ribOut_sources` / `ribVector` source lists | 6 Core files in `ribOut`; 7 `render/` plus 4 `vector/*Stub.cpp` in `ribVector` | from the generated `build.make`: `ribOut_obj` compiles `render/{rendererDeclarations,rendererFileSearch,rendererMutexes,rendererNetwork,rendererStatics}.cpp` and `vector/rendererDeclarationsDispatchStub.cpp` (6 Core); `ribVector_obj` compiles `render/{rendererClipping,rendererDeclarations,rendererFileSearch,rendererMutexes,rendererNetwork,rendererStatics,resource}.cpp` (7) and the four `vector/*Stub.cpp` | equal |
| e | M5 `nm` sweep (undefined symbols of every non-Core `ribVector` object and `libribpreview` object joined with the 355 symbols the 11 Core objects define) | M5 table | `rendererDeclarations`: `dataLoad`, `blobby`, `parse/ri`, `previewContext`, `rib_generated`; `rendererFileSearch`: `blobbyRepeller`, `rib_generated`; `rendererStatics`: `dataLoad`, `blobbyRepeller`, `object`, `compositor`, `irradiance`, `photonMap`; `geometryDispatchStub`: 12 Geometry objects; `irradianceDispatchStub`: `irradiance`; `rendererDeclarationsDispatchStub`: `object`; `surfaceCrossingDispatchStub`: `bsplinePatchgrid`, `patches`, `patchgrid`, `subdivision`; `rendererMutexes`, `rendererNetwork`, `rendererClipping`, `resource`: no non-Core referencer | equal to M5 (the post-026 build changes nothing). The only Core symbols `libribpreview` references come from `previewContext.cpp.o` (`initDeclarations`, `shutdownDeclarations`). |
| g | `renderMan = ...` outside `src/ri/parse/` | M6 | `previewContext.cpp:200,202` (+ comment `:196`), `dataviewer/dataLoad.cpp:142,175`, `render/rendererContext.cpp:6226` (+ `savedRenderMan` `:6225,6232`, `extern`s `:100-101`); tests `blobbyTestUtils.h:53`, `test_texture_system_stream_cursor.cpp:111,229`, `test_texture_system_null_resolver.cpp:115,165` (+ comment `:14`), `test_data_keys.cpp:51,57`. Inside `parse/`: `ri.cpp:111,287,311,313,322,460,2141`, `riInterface.cpp:37,57` | equal to M6 (the check's regex also matches the two comments at `previewContext.cpp:196` and `test_texture_system_null_resolver.cpp:14`, which the Phase 3 tasks already reword) |

### 0.5 Cited line numbers (T008 f)

Every site cited in contracts/ and in tasks.md was opened on the merged tree. All are at the cited lines (`ri.cpp` 40, 46, 110, 255-257, 287-289, 311-324, 407-417, 459-460, 2139-2145; `rib.y` 37-40, 222, 232-245; `rib.l` 170, 178; `ifexpr.y` 28-29, 203-224, 568; `rendererContext.cpp` 70, 100-101, 112, 6224-6233, 6399, 6421; `rendererContext.h` 274; `riInterface.cpp` 37-39, 56-59; `llvmEmitter.cpp` 173/210 (tables in the cited 159-211), 502-503, 1101-1105; `sloLoader.cpp` 139; `rendererServices.h` 404; `libshader/compiler/expression.cpp` 1534-1557; `execute.cpp` 601-625; `opcodes.cpp` 191; `rendererFileSearch.cpp` 72-148; `renderer.cpp` 135; `rendererStatics.cpp` 109-122; `renderer.h` 152, 157, 158; `rendererServicesImpl.h` 41-42; the cited test sites) with these exceptions, corrected in the documents:

- `src/ri/CMakeLists.txt`: the `ifexpr_generated.cpp` exclusion from `ribVector` is at `:383`, not `:381` (tasks.md T064, spec.md edge-case text); `ribOut_sources` starts at `:515`, not `:513` (spec.md); the `ribVector` source block is `:364-408`, not `:363-406` (spec.md). `research.md` already says `:364-408` and `:515-530`.

Clarification offered for the Phase 2 TEST-APPROVAL stop (not a defect, no document changed): `ri.cpp:407-417` bootstraps `CRenderer::initDeclarations()`/`globalArena.init()` and `globalArena.tini()`/`shutdownDeclarations()` only `if (ribOutMode)`. T062 and contracts/scene-services.md replace the block with `{ CRiStandardSession session; globalArena.init(); ribParse(rcPath, NULL); globalArena.tini(); }` "in the same scope as today"; the renderer path (`ribOutMode` false) already initializes both in `beginRenderer()`, so the replacement is the writer branch only (T062 already calls it that) and must stay conditional on `ribOutMode`; the stop can state it so that the unconditional reading in the contract's one-line table is not implemented.

### 0.6 Linux tree and baseline build (T009)

- Created `~/Projects/Development/CLI/openrender-038` on `juvenal@ubuntu-2404-desktop.v2-labs.io`; synced 3500 tracked and new files with `rsync -rlpc -0 --files-from=$WT/build-038-snapshots/files.list` (no `-t`), `sha256sum -c --quiet manifest.sha256` printed OK. The snapshot directory `build-038-snapshots/` is under the gitignored `build-*/` pattern.
- `cpujob 038-lx-cfg cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DINSTALL_SELFCONTAINED=OFF` exit 0; `cpujob 038-lx-build cmake --build build -j2` (`CPUJOB_TIMEOUT=7200`, background, log `lx-build.log` on the remote): finished with 0 `warning:` lines and no error (GCC 13, `-Werror`).

### 0.7 Checkpoint 0, macOS (T011)

- `cpujob 038-p0-all ctest --test-dir $WT/build -LE determinism-sweep -j4 --output-on-failure` (log `$WT/build/p0-all.log`): **100% tests passed out of 1052** in 311.7 s; one test skipped by design, `LibShader_Format_rslo_lower_minor`. No failure, no flaky test seen in this run. `ctest -N` lists 1471 tests in total, so the sweep is 419 (1471 - 1052), matching `CLAUDE.md`.
- `cpujob 038-p0-arch2 ctest -L architecture` (log `p0-arch2.log`): 21/21 passed. archcheck: `platform=macos objects=295 sources=200 edges=2544 (include=1639 link=905)`, `PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`, METRIC `SceneInterface 6/6`, `TOTAL 612/94`.
- `cpujob 038-pN-arch38 ctest -L or038_red`: "No tests were found" (the label is not yet used; expected at Phase 0).

### 0.8 Checkpoint 0, Linux (T012)

- `cpujob 038-p0-lx-test ctest --test-dir build -LE determinism-sweep -j2 --output-on-failure` (`CPUJOB_TIMEOUT=7200`, log `lx-p0-all.log` on the remote): **99% tests passed, 7 failed out of 1053** in 680 s (one more test than macOS: `IPCDisplayTests`, Linux only; `LibShader_Format_rslo_lower_minor` skipped as on macOS).
- The seven failures are the known open Linux issues. Each was re-run alone with its own `cpujob` call and failed again (second column):

| Test | Full run | Alone | Issue |
|---|---|---|---|
| `LibShader_McompConsistency` | Failed | Failed | #44: `libshader_shading.so.1: undefined symbol: _ZTV19CTransmissionBundle` at load (the issue title names `typeinfo for CFileResource`; the same failure class, a different missing symbol) |
| `TextureTile_RenderByteIdentical` | Failed (output differs from reference, byte 5, several scenes) | Failed | #45 |
| `TextureTile_CacheByteIdentical`, `_CacheDefaultDirectory`, `_CacheStaleDetection`, `_CacheConcurrentWrite` | Failed | Failed | #46 |
| `CoverageAnalytic_alpha-sides2-tjunction-r4_reyes` | Failed (`pixel (99, 46): value 0.875, expected 0.9375`) | Failed | #57 (the issue's other scene, `sphere-occlusion-reyes`, passed: `Visual_sphere-occlusion-reyes` and `-slo`) |

  #44 is expected to remain failing in 038. These seven are the Linux baseline for every later checkpoint.
- `cpujob 038-p0-lx-arch ctest -L architecture`: 21/21 passed. archcheck: `platform=linux objects=884 sources=203 edges=2549 (include=1644 link=905)`, `PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`, `SceneInterface 6/6`, `TOTAL 612/94`. `-L or038_red`: no tests.

### 0.9 Summary

TEST-APPROVAL (Phase 0, T010): no approved test, check or reference was edited or added. Files this phase changed: the spec documents (D4-a and the line-number corrections of 0.2 and 0.5) and this file. No count in the documents differed from the measurement. Nothing else is open from Phase 0; the maintainer commits the staged documents.

## 1. Phase 1: tests first, references, Linux gate

### 1.1 Measurements (T014 to T019)

Tools written for them live under `specs/038-scene-interface-untangle/tools/`: `r14/` (the probe shaders and `r14a.sh`), `ribvector_core_sweep.sh`, `include_sweep.sh`, `ctor_reports_sweep.sh`. Every run went through `cpujob` (labels `038-p1-r14a`, `038-p1-r14b`, `038-p1-sweeps`).

**R14a measured (T014): DEFECT FOUND on the `.rslo` path, to file as a P1.**

`oshader` has no switch that turns a pass off (the five passes, ConstFold, UniformLifting, CSE, DCE, CallSites, are added unconditionally in `rslo.cpp:1676-1682`, and `OPENRENDER_DUMP_IR` prints only the pass names, not the IR), so the "passes off" side of the comparison is the shader source: the expected DSO lines are the calls in source order, with their own arguments. Five probes (`tools/r14/r14a_*.sl`), compiled against the spec 037 `has_exec` fixture, output of `tools/r14/r14a.sh` (log `build/p1-r14a.log`):

| Probe | Result |
|---|---|
| `r14a_dup` two identical calls in a row | both `DSO` lines kept, in order (the second reuses `temporary_0`) |
| `r14a_unused` a call whose result is never read | the `DSO` line is kept (DCE leaves it: `DSO` is not in its pure list) |
| `r14a_readback` `v = 2.0; v = probe_add(..); w = v + 1.0` | the constant 2.0 is not propagated across the call; `v` is reloaded from the destination |
| `r14a_loop` a call in a `for` body | kept, inside the loop |
| `r14a_diffargs` two calls with different arguments | **defect**, below |

The defect: `CCSEPass` (`passes/passCSE.cpp`, `cseFn`) turns the second `vufloat b temporary_0` into `moveff b a`:

```text
	DSO  probe_add ("f=ff") temporary_0 0.25 0.5
	vufloat  a temporary_0
	DSO  probe_add ("f=ff") temporary_0 0.1 0.2
	moveff  b a            <- was: vufloat b temporary_0
```

For a `DSO` line `irBuilder` records the operand's name (`probe_add`) as the instruction's result and the destination (`temporary_0`) as an operand, so `invalidateVar(exprMap, instr.result)` invalidates nothing for `temporary_0`, and the cached expression `vufloat|temporary_0 -> a` outlives the second call that rewrote `temporary_0`. Rendered (`orender`, `.rslo`, one pixel, `tools/r14/r14a.sh`): `Ci = color(a, b, 0)` with `a = probe_add(0.25, 0.5)` and `b = probe_add(0.1, 0.2)` gives the centre pixel `R=191 G=190 B=0` where `G` should be `76` (0.3): `b` reads `a`. Two uniform DSO calls in one shader, with different results and the same destination temporary, return the first call's value twice. A varying call writes its own variable (`DSO ... v_1 k temporary_0`), so it is not affected; the interpreter and the JIT consume the same post-pass IR, so a `.rslo`/`.slo` parity test cannot see it. Not reproduced silently and not fixed here (tasks.md T014); the new shaders of this phase (`probe_args.sl`, `probe_init.sl`) contain at most one uniform call each, so none of their results depends on it. **The approved `probe_two.sl` has the pattern**: its compiled code (`build/tests/plugins/shadeop/shaders/probe_two.rslo`) is `DSO probe_add ("f=ff") temporary_0 0.25 0.5; vufloat v_1 temporary_0; DSO probe_mul ("f=ff") temporary_0 2.0 0.375; moveff w v_1`, so `w = probe_mul(2.0, 0.375)` reads `v`. It is hidden because both operands return 0.75, so `ShadeopAbi_Versions_Render`, `ShadeopAbi_LegacyShared` and the new `ShadeopAbi_Jit_ProbeTwo` (which compares `.rslo` and `.slo`, both built from the same post-pass IR) cannot see it. `probe_two.sl` is approved and was not edited. Filed as **GitHub #139 (P1)** at the T049 approval; the Phase 6 gate was cleared there, `probe_two.sl` stays unchanged, and the fix's regression test belongs to #139.

**R14b measured (T015): a shadeop prototype cannot contain an upper-case letter.** `parseShadeopDeclaration` (`src/shadeop/shadeopDeclaration.cpp`) builds the prototype only from `typeLetter()`, which returns `o f v n p c m s`; the word `output` is skipped and no parameter or return word with an upper-case letter is a type (`Float`, `FLOAT`, `Vector` scan as names and the declaration is refused). `expression.cpp:1534-1557` promotes a uniform operand to varying unless `usedPrototype[i + 2]` is `A` to `Z`, and for a DSO call `usedPrototype` is built from the lower-case letters of the declaration (`CBuiltinExpression::getCode`, branches `'f'` to `'s'`), so every operand of a varying DSO call is varying. Checked by running the approved characterization (`cpujob 038-p1-r14b ctest -R ShadeopAbi_Declaration`: 1/1 passed) and, in a scratch run of the same program with `tools/r14/r14b_declarations.txt` (`CPUJOB_TIMEOUT=60 cpujob 038-p1-r14b-scratch test_shadeop_declaration <file>`, the approved `declarations.txt` is not edited): `float f(float F)`, `float f(float a, vector V)` and `float f(output float OUT)` are accepted with prototypes `f=f`, `f=fv`, `f=f`; `Float`, `FLOAT`, `OUTPUT`, `Vector`, `String`, `Color` as type words are refused; 10 cases, 0 failed. No defect.

**R14c measured (T016): the JIT's uniform decision equals the loader's.** `.rslo` loader (`rslo.y`, `rsloOperand`): `opcodeUniform` starts `TRUE` at the opcode (`:1847` for `DSO`) and becomes `FALSE` for the first operand that is a shader variable with `uniform == FALSE` (`:494-495`) or a global whose container is neither uniform nor constant (`:513-515`); constants are uniform; the destination is one of the operands. JIT (`llvmEmitter.cpp`, `collapseArgs`, `:929`): uniform iff `dstStrideVal == 0` and every operand stride is 0, where `buildVarTable` gives stride 0 exactly for `SLC_UNIFORM` variables (parameters, locals and globals alike) and literals are stride 0. Same predicate. One difference to carry into Phase 6, not a defect today: in a masked scope (`conditionalDepth > 0`) `collapseArgs` runs a uniform call `n = (numActive != 0) ? 1 : 0` times with no tags, where the interpreter jumps over the block when no point is active and runs it once otherwise; the two agree. `probe.sl` (compiled uniform: `DSO probe_add ("f=ff") temporary_0 0.25 0.5`) and `r14c_vary.sl` (varying: `DSO probe_add ("f=ff") v_1 k temporary_0`, and under an `if`: `DSO probe_add ("f=ff") w t temporary_1`) cover both answers. Phase 6 note: contracts/jit-dso-call.md says `steps` are "per-vertex byte sizes (VarDesc item bytes)"; the interpreter's step for a uniform operand is 0 (`varyingStep`), and `VarDesc::itemBytes` is non-zero for a uniform variable. Because R14b shows every operand of a varying call is varying, the two cannot differ for a DSO call; the emitter should still pass `stride == 0 ? 0 : itemBytes`.

**T017 survey (tests that construct a context without `RiBegin`).** `git grep` over `tests src/preview src/ri/dataviewer`:

| Site | Constructs | Today | After Phase 3 |
|---|---|---|---|
| `tests/unit/blobby/blobbyTestUtils.h:51-54` | `new CRiInterface`, `renderMan = iface`, `RiErrorHandler(recordError)` | relies on the constructor attaching diagnostics (M6) | `riInstallContext` once (approved edit, listed) |
| `tests/unit/texture_tile/test_texture_system_stream_cursor.cpp:110-111,228-229` | `new CRiInterface`, assign, delete, restore | relies on the constructor's assign and attach | install / delete (listed) |
| `tests/unit/texture_tile/test_texture_system_null_resolver.cpp:114-115,164-165` | same | same | install / delete (listed) |
| `tests/preview/test_data_keys.cpp:50-51,56-57` | same | same | install / delete (listed) |
| `src/ri/dataviewer/dataLoad.cpp:141-142,175` | `renderMan = new CRiInterface()` | production | `riInstallContext` (T077) |
| `src/preview/libribpreview/previewContext.cpp:195-204` | `CPreviewContext ctx;` on the stack, `renderMan = &ctx` | production | back-end session (Phase 4) |
| `tests/unit/blobby/test_surface_params.cpp:102`, `tests/unit/csg/csgTestUtils.h:129`, `tests/unit/motion/replay_capture.h:88`, `test_frame_params_equal_core.cpp:177`, `tests/unit/sampling/scene_probe.h:103` | subclasses made by `RiSetContextFactory` | constructed inside `RiBegin`, which installs them | unchanged (no edit): `RiBegin` calls `riInstallContext` |

**No site beyond research M6's four tests (plus the two production sites M6 already lists).** The `tests/shading_contract` and `tests/shading_parity` harnesses only name `CRiInterface *` as a parameter type. Nothing constructs a `CRendererContext` or `CRibOut` directly.

**Sweeps (T018, T019), macOS merged build, `cpujob 038-p1-sweeps`:**

- `ribvector_core_sweep.sh` (log `build/p1-sweep-ribvector.log`): RESIDUAL = 9 Core files, equal to the 9 lines of contracts/allowlist-end-state.md: `rendererDeclarations`, `rendererFileSearch`, `rendererMutexes`, `rendererNetwork`, `rendererStatics`, `geometryDispatchStub`, `irradianceDispatchStub`, `rendererDeclarationsDispatchStub`, `surfaceCrossingDispatchStub`; UNREFERENCED: `rendererClipping.cpp` and `resource.cpp` (Q7). On the starting tree the referencers of `rendererDeclarations` include `parse/ri.cpp`, `rib_generated.cpp`, `dataviewer/dataLoad.cpp` and `previewContext.cpp` (all removed by Phases 2 to 4) besides `geometry/blobby.cpp`; `rendererFileSearch` is referenced by `rib_generated.cpp` and `blobbyRepeller.cpp`.
- `include_sweep.sh` (log `build/p1-sweep-include.log`): 146 resolved includes, 13 Core: `rib.y`, `ifexpr.y` (`renderer.h`, `rendererContext.h`) and the generated parsers that copy their prologues, `parse/ri.cpp` and `output/ribOut.cpp` (`render/riHooks.h`), and `previewContext.cpp` (`renderer.h`) and `ribGeometryContext.cpp` (`renderer.h`, `rendererContext.h`). As expected: nothing in `src/ri/parse`, `src/ri/output` beyond `rib.y`, `ifexpr.y` and the two `riHooks.h` lines.
- `ctor_reports_sweep.sh` (log `build/p1-sweep-ctor.log`): the bodies of `CRendererContext`'s constructor, `beginRenderer`, `initMutexes`, `initFiles`, `initDeclarations`, `rendererShadingServicesInit`, `rendererProceduresInit` and `initNetwork` hold one report call, `warning(CODE_SYSTEM, "Atomic operations are not supported on this system ...")` in `initMutexes`. It does not follow the callees; Phase 3's T072 reads them.


### 1.2 What Phase 1 added (T020 to T044)

No existing approved test, check or reference was edited. The only edits to existing files are registrations: `tests/CMakeLists.txt` (the capture target, `OR038_*` options, `add_subdirectory` of `unit/scene_interface` and `precomp`), `tests/architecture/CMakeLists.txt`, `tests/preview/CMakeLists.txt`, `src/libshader/tests/CMakeLists.txt`, `tests/plugins/shadeop/CMakeLists.txt`.

| Kind | Name | Files |
|---|---|---|
| mechanical check, `or038_red` | `SceneInterface_RibOutNoCore`, `_RibVectorResidual` (+ `ribvector-core-residual.txt`, 9 lines), `_NoCoreInclude`, `_HooksAndRecords`, `_ActiveContextAssignments`, `Compiler_IncludeBoundary` | `tests/architecture/check_*.cmake`, shared `tests/architecture/domain_classify.cmake` (reads `domain-map.txt` with archcheck's rule order) |
| mechanical check, `or038_red` | `Preview_NoCoreReach` (also `preview`), `Precomp_NoHiderCore` | `tests/preview/check_preview_no_core_reach.cmake`, `tests/precomp/check_precomp_no_hider_core.cmake` |
| characterization (green) | `SceneInterface_WriterText` (8 cases, `tests/RIB/writer/*.rib` + `include/`), `SceneInterface_IfCharacterization` (`scenes/if_branches.rib`), `Preview_ExtractionUnchanged` (14 scenes), `Precomp_OutputUnchanged`, `Shaders_BytesUnchanged` | `tests/unit/scene_interface/`, `tests/preview/test_extraction_unchanged.cpp`, `tests/precomp/check_precomp_output.cmake`, `src/libshader/tests/check_shaders_bytes.cmake` |
| red at run time | `SceneInterface_ContextInstall` (rule 1) | `tests/unit/scene_interface/test_context_install.cpp` |
| `DISABLED`, `EXCLUDE_FROM_ALL`, compile failure | `SceneInterface_ContextInstallRules` (same source, `-DSI_INSTALL_RULES`), `SceneInterface_Services`, `Preview_SelfRegistration` | `test_context_install.cpp`, `test_services.cpp`, `tests/preview/test_self_registration.cpp` |
| `DISABLED`, mutation self-check | `LibShader_CompilerTableDrift` | `src/libshader/tests/test_compiler_table_drift.cpp` |
| red (`.slo` compile exits 3) | `ShadeopAbi_Jit_Probe`, `_ProbeTwo`, `_Args`, `_Void`, `_Init`, `_PerThread8`, `_MissingOperand` | `tests/plugins/shadeop/{jit_fixture.cpp,probe_args.sl,probe_void.sl,probe_init.sl,render_format.rib.in,check_jit_parity.cmake}` |
| capture target | `scene-interface-capture-baselines` (not `all`, not a ctest), options `OR038_RECAPTURE`, `OR038_CAPTURE_DIFFERENCES` | `tests/CMakeLists.txt` and one `si_capture_*` step per directory |
| program | `capture_scene_interface_declared_set` (writes `reference/declared_set.txt`: 121 variables in list order, 29 globals, from the unmodified `CRenderer::initDeclarations()`; `entry` is written for globals only, see 1.7) | `tests/unit/scene_interface/capture_declared_set.cpp` |

Decisions and deviations from tasks.md, each for the maintainer:

1. The temporary label `or038_red` collides with `ctest -L architecture`: `-L` is a regular expression and `architecture` matches the substring of `or038_red`. `ctest -N -L architecture` lists 29 tests, the 21 existing plus the 8 red checks, so `ctest -L architecture` fails while any check is red. `-L '^architecture$'` selects exactly the 21 existing tests. Not renamed (the label is tasks.md's); see section 1.6.
2. `LibShader_CompilerTableDrift`'s executable is NOT `EXCLUDE_FROM_ALL`: its source lives under `src/libshader/tests/`, which `domain-map.txt` classifies Consumer, so archcheck lists it among the in-scope objects, and an unbuilt object makes `Architecture_DomainBoundaries` exit 2 ("incomplete build tree"). It compiles and links today, so it is in the normal build; only the ctest entry is `DISABLED`. The other three disabled executables are under `tests/` (out of archcheck's scope) and are `EXCLUDE_FROM_ALL` as tasks.md says.
3. The `--jit` compiles of the `ShadeopAbi_Jit_*` cases run inside `check_jit_parity.cmake` at ctest time, not as build steps, so the default build stays green (tasks.md T044 says "the extra `oshader --jit` compile commands" belong to the registration).
4. `Preview_SelfRegistration` asserts, for `RiBegin("file.rib")` in a program that links `libribpreview`, the unchanged behaviour (a writer becomes active and writes the file), not the "RIB-output mode is not available in this build" text that T037 and FR-014 name: `libribpreview` links `libRIBVector`, which compiles `output/ribOut.cpp`, whose static initializer registers the RIB writer (`nm libRIBVector.dylib` shows `g_ribOutRegistrar`). Only a program that links neither `libRIBVector` nor `libRIBOut` can print that text. `RiBegin("#x")` prints the "Full-render mode is not available" text as specified. The spec, FR-014 and T037 need the maintainer's decision.
5. `Preview_ExtractionUnchanged` covers 14 scenes: `camera-dof`, `polygon`, `general-polygon`, `quadrics-a`, `quadrics-b`, `colorcube`, `teapot` (patches, with `GEOMETRIES` set to the tracked `geometry/`), `subdiv-geometry-samples`, and the six wire scenes of `test_preview_subdiv.cpp`. `nurbs-trim-sample.rib` was tried and dropped: it extracts nothing on the starting tree (`Required parameter "Pw" is missing`).
6. `SceneInterface_WriterText` also keeps a `<case>.diag` per case next to `<case>.rib` (the diagnostics, one per line), and its `procedurals` case writes no Procedural statement on the starting tree (`CRibOut` writes nothing for them): the reference characterizes that. `inline_params` shows that `Declare "myweight" ...` is not seen by the parser when the writer is the context ("Parameter \"myweight\" is not declared"), because `CRibOut` keeps its own table.
7. `SceneInterface_IfCharacterization` records quirks of the evaluator as they are, none of them fixed: of the 15 Spheres taken, branches 41 and 43 (`&&` with `$Option:limits:bucketsize`, `||`) and 51 (`=~`) and 55 (`defined($user:level)`) are not the ones a reading of the expression suggests, and the diagnostics hold two "Unable to cast a string in the expression" and a "Failed to find variable" inside `defined()`. The reference pins today's behaviour so Phase 2 can be compared with it; whether any of these is a defect to file is the maintainer's call.
8. Reference lookup: macOS references are the primary ones; on Linux a file of the same name under `reference/linux/` wins where it exists, and `OR038_CAPTURE_DIFFERENCES=ON` makes the capture target write such a file only for a result that differs.

### 1.3 R10a measured (T040)

`test_libshader_compiler_table_drift --measure-opcodes` (`cpujob 038-p1-r10a`, `CPUJOB_TIMEOUT=60`): the engine has 96 opcode rows with 89 distinct texts (several arities share a text); the compiler's `kAllOpcodeMnemonics` has 95 entries, 95 distinct after `stripOpcodeMnemonic`. Engine-only: none. Compiler-only: six, `beginilluminance`, `moveaff`, `moveavv`, `moveass`, `moveamm` (all in `kDeadOpcodes`, the compiler never emits them) and `gatherHeader` (emitted in function form and implemented by the engine as a function row, `giFunctions.h` `DEFSHORTFUNC(GatherHeader, "gatherHeader", ...)`, which `kAllFunctionMnemonics` compares). The comparator encodes the six by name (`kCompilerOnlyOpcodes`) with the reasons; `kEngineOnlyOpcodes` is empty. With the exceptions, the real tables compare equal (as they must before Phase 5: both sides come from the same headers).

Mutation self-check (`--self-check`, the test's red evidence, log `build/p1-drift-run.log`), every mutation reported:

```text
self-check: "one row's text changed (kOpcodeParamTable)" reported: row 139: compiler ("Deriv_mutated", 0x00007030) engine ("Deriv", 0x00007030)
self-check: "one row's bits changed (kOpcodeParamTable)" reported: row 208: compiler ("transform", 0x00000001) engine ("transform", 0x00000000)
self-check: "one row missing (kOpcodeParamTable)" reported: row count: compiler 416 engine 417
self-check: "one row's text changed (kAllFunctionMnemonics)" reported: row 160: compiler "noise_mutated" engine "noise"
self-check: "an opcode renamed (kAllOpcodeMnemonics)" reported: the engine has an opcode "addff" the compiler's kAllOpcodeMnemonics does not list (and it is not a named exception)
```

`--emit` prints the 417-row `builtinTables.cpp` (766 lines; `{"visibility", PARAMETER_RAYTRACE | PARAMETER_DERIVATIVE | PARAMETER_N | ...}` spelled as the engine does).

### 1.4 Captured references (T046)

`cpujob 038-capture cmake --build build --target scene-interface-capture-baselines -j1` on the unmodified tree, then a second run: 0 captured, 35 kept, `shasum` of the 35 reference files then existing identical before and after (that check predates the recapture of `declared_set.txt` described in 1.7; the recapture was run by the same target after deleting that one file, and the five `reference/linux/` files were added by the Linux gate, so there are now 40 reference files). No reference holds a machine path. 35 files: `tests/unit/scene_interface/reference/{declared_set.txt,if_branches.txt,writer/*.rib,writer/*.diag}` (2 + 16), `tests/preview/reference/macos/*.txt` (14), `tests/precomp/reference/stochasticPrimitives.raw.h`, `src/libshader/tests/reference/macos/{shaders.sha256,oshader_help.txt}`.

### 1.5 Red evidence (T047)

`cpujob 038-red ctest --test-dir build -R 'SceneInterface_|Preview_|Compiler_IncludeBoundary|Precomp_|ShadeopAbi_Jit_|Shaders_BytesUnchanged|LibShader_CompilerTableDrift' -j4 --output-on-failure` (log `build/p1-red.log`): 21 tests, 5 passed (the characterizations `SceneInterface_WriterText`, `_IfCharacterization`, `Preview_ExtractionUnchanged`, `Precomp_OutputUnchanged`, `Shaders_BytesUnchanged`), 12 failed, 4 `Not Run (Disabled)` (`Preview_SelfRegistration`, `SceneInterface_ContextInstallRules`, `_Services`, `LibShader_CompilerTableDrift`). The reason of each red test:

| Test | Failure |
|---|---|
| `SceneInterface_RibOutNoCore` | `ribOut_obj` compiles 6 Core files: `rendererDeclarations`, `rendererFileSearch`, `rendererMutexes`, `rendererNetwork`, `rendererStatics`, `vector/rendererDeclarationsDispatchStub` |
| `SceneInterface_RibVectorResidual` | `ribVector_obj` compiles `rendererClipping.cpp` and `resource.cpp`, which the list does not name (11 compiled, 9 listed) |
| `SceneInterface_NoCoreInclude` | `rib.y` and `ifexpr.y` include `renderer.h`/`rendererContext.h`; `ri.cpp` and `ribOut.cpp` include `render/riHooks.h`; so do the generated parsers (10 includes) |
| `SceneInterface_HooksAndRecords` | `riHooks.h` is in `render/` and not `parse/`; `CDelayedData` is defined in `geometry/delayed.h`; `ri.cpp`, `rib.y` and `rib_generated.cpp` include `delayed.h` |
| `SceneInterface_ActiveContextAssignments` | 17 matches in `previewContext.cpp`, `dataLoad.cpp`, `rendererContext.cpp` (with `savedRenderMan`), `test_data_keys.cpp`, `blobbyTestUtils.h`, the two texture_tile tests (research M6, comments included) |
| `Preview_NoCoreReach` | `previewContext.cpp` and `ribGeometryContext.cpp` include Core headers; `previewContext.cpp.o` needs `CRenderer::initDeclarations` and `shutdownDeclarations` |
| `Compiler_IncludeBoundary` | the include path holds `src/ri`, its 9 subdirectories and `src/libshader/shading`; `llvmEmitter.cpp` includes `scriptFunctions.h` and `scriptOpcodes.h` |
| `Precomp_NoHiderCore` | `precomp.cpp` includes `reyes.h` and `renderer.h` |
| `SceneInterface_ContextInstall` | three printed CHECK failures (rule 1): the constructor made the context active, attached the diagnostics, and an `error()` did not reach the earlier sink; no crash |
| `ShadeopAbi_Jit_*` (7) | `oshader --jit <shader>.sl` exited 3: `llvmEmitter: JIT coverage gap for '<shader>': builtin 'DSO' has no emitFunction() case (kHandledOpcodes[] is missing it)` (for `probe_init` the gap is in `probe_init_init`, the init function). The `.rslo` compile of the same shaders succeeded and, in a scratch run of the `.rslo` halves, the scenes render: `Void` logs 24,544 records, `PerThread8` makes 8 `threadInit` calls on 8 distinct threads |

The three disabled API tests fail to compile (`cpujob 038-p1-red-<target> cmake --build build --target <target>`, logs `build/p1-red-*.log`), each naming the missing API:

- `test_scene_interface_services`: `test_services.cpp:55:10: fatal error: 'declarations.h' file not found` (the first missing header, `state/declarations.h`; `riServices.h` and `riCondition.h` follow it);
- `test_scene_interface_context_install_rules`: `use of undeclared identifier 'riInstallContext'` (and `riPreviousContext`, `riUninstallContext`);
- `test_preview_self_registration`: `use of undeclared identifier 'RiBeginBackEnd'` (and `RiEndBackEnd`).

`LibShader_CompilerTableDrift`'s red evidence is the self-check of 1.3.

The test bodies of the three disabled tests could not be run: their API does not exist, so their behaviour past the includes is checked only by reading until Phase 2, 3 and 4 enable them.

### 1.6 Architecture check (macOS)

- `cpujob 038-p1-arch ctest --test-dir build -L '^architecture$'`: 21/21 passed (`Architecture_DomainBoundaries` included, the new drift-test object being an in-scope Consumer).
- `cpujob 038-p1-arch2 ctest --test-dir build -L architecture`: 29 tests, 8 failed, exactly the 8 `or038_red` checks above. **`ctest -L architecture` does not pass** while the label `or038_red` exists, because of the regular-expression match. The decision is the maintainer's: rename the temporary label to one that does not contain `architecture` (for example `or038_red`), or anchor `-L '^architecture$'` in every checkpoint command and in tasks.md Convention 19; Phase 9's "no test carries `or038_red`" works either way.

### 1.7 Linux gate (T048) and platform differences

Tree `~/Projects/Development/CLI/openrender-038` on `juvenal@ubuntu-2404-desktop.v2-labs.io` (Ubuntu 24.04, GCC 13, LLVM 18, `-Werror`, `-j2`), synced with `rsync -rlpc -0 --files-from` and verified with `sha256sum -c` (3,587 files, OK after each sync). Build: `cpujob 038-p1-lx-build cmake --build build -j2`, 0 `warning:` lines, exit 0 (all the new tests and the disabled drift executable compile under GCC 13 `-Werror`; the three `EXCLUDE_FROM_ALL` API tests were not built, as on macOS). Logs on the remote: `lx-p1-build2.log`, `lx-p1-first/` (the first run, before the fix below), `lx-p1-red-compare.log`, `lx-p1-capture.log`, `lx-p1-red-after.log`, `lx-p1-arch.log`, `lx-p1-run.sh`.

The red set behaves as on macOS: the 8 `or038_red` checks, `SceneInterface_ContextInstall` and the 7 `ShadeopAbi_Jit_*` tests fail for the same reasons, the 4 disabled tests are `Not Run (Disabled)`, `SceneInterface_WriterText`, `_IfCharacterization` and `Precomp_OutputUnchanged` pass against the macOS references unchanged. `ctest -L '^architecture$'`: 21/21 passed (`archcheck` on Linux).

Differences between the macOS and Linux results, each captured as `reference/linux/<file>` with `-DOR038_CAPTURE_DIFFERENCES=ON` (`scene-interface-capture-baselines` on the Linux tree, fetched back with `rsync` and checked with `sha256sum`); the lookup uses the Linux file where it exists and the macOS one otherwise:

| File | Why it differs |
|---|---|
| `tests/preview/reference/linux/examples_rib_camera-dof.txt` | 6 of 103 lines: the last bit of a float (`0.164590999` against `0.164591014`): GCC and Clang contract multiply-adds differently (CLAUDE.md gotcha 26) |
| `.../examples_rib_quadrics-a.txt` (307 of 1519 lines), `.../examples_rib_quadrics-b.txt` (1470 of 5143), `.../examples_rib_teapot.txt` (173 of 1543) | the same cause: last-bit floats and sub-1e-16 residuals (`1.77302326e-17`) in the tessellation of quadrics and patches |
| `src/libshader/tests/reference/linux/shaders.sha256` | 130 lines, all the `.slo` files: the bitcode follows the LLVM version (18 on Linux, the vcpkg LLVM on macOS). The 132 `.rslo` lines (130 shipped shaders, `probe`, `probe_two`) and `oshader_help.txt` are identical |

The other eight preview scenes, all the writer texts and diagnostics, `if_branches.txt`, `declared_set.txt` and `stochasticPrimitives.raw.h` are identical on both platforms (no `linux/` file).

One defect found on the way, in the first Linux run: `declared_set.txt` differed from line 3 (`entry=117620528` on Linux where macOS read `entry=1`) for every non-global variable. `CVariable::entry` is documented "-1 if not global" but the declaration code never initializes it for non-globals (`declareVariable` copies a stack `CVariable` whose `entry` was not set, `parseVariable` does not set it), so the macOS `1` was a coincidence. The reference now writes `entry` for global variables only (`entry=-` otherwise); the macOS reference was recaptured (the only reference recaptured; deleted and rewritten by the target) and the Linux one removed. This is a latent uninitialized read in `src/ri/render/rendererDeclarations.cpp` / `src/common/variable.cpp`; not harmless: `CChannelRegistry::declare(CVariable *)` (`src/ri/state/channelRegistry.cpp:104`) reads `var->entry` unguarded when it builds a `CDisplayChannel`, so a channel declared over a non-global variable carries whatever the allocation held. Filed as **GitHub #140 (P1)**; not fixed by 038 (the `entry=-` form of the reference stays, so the reference does not depend on it).

Linux baseline failures (#44, #45, #46, #57) are outside the tests this phase ran (`-R` set above plus `-L '^architecture$'`); the full `-LE determinism-sweep` suites were not run (T050 and T051 are not part of this stop).

### 1.8 T049 approval: decisions and departures (2026-11-02)

The maintainer approved T049 with these decisions, applied before the Phase 1 checkpoint:

1. **Label renamed `architecture_038` -> `or038_red`** (departure from tasks.md Convention 19 as first written). Reason: `ctest -L` is a regular expression and the old label contained `architecture`, so `ctest -L architecture` selected the 8 red checks too (29 tests, 8 failing). Changed in the CMake label strings (`tests/architecture/CMakeLists.txt`, `tests/preview/CMakeLists.txt`, `tests/precomp/CMakeLists.txt`) and in tasks.md (Convention 19, T049, every checkpoint `-L` command, Phase 9's check), quickstart.md, plan.md and contracts/checks.md. Sections 1.2 to 1.6 above were written with the old name and are corrected to the new one by the same substitution; this paragraph is the one place the old name is kept. Phase 9 requires `ctest -N -L or038_red` to list nothing. Checkpoint result in 1.9.
2. **R14a CSE defect: GitHub #139 (P1).** Recorded in 1.1, research R14a, plan risk table and contracts/jit-dso-call.md. The Phase 6 gate is cleared; `probe_two.sl` stays unchanged; the regression test of the fix belongs to #139.
3. **FR-014 / T037 corrected to match the code.** A program that links `libribpreview` links `libRIBVector`, which registers the RIB writer, so `RiBegin("file.rib")` writes the file as today; only `RiBegin("#x")` reports "Full-render mode is not available in this build". Edited: spec.md (FR-014 and acceptance scenario 3 of the preview story), tasks.md T037, contracts/backend-registration.md, contracts/checks.md. `Preview_SelfRegistration` already asserts this (no test edit).
4. **Accepted departures from tasks.md wording:** (a) `LibShader_CompilerTableDrift`'s executable is in the normal build (archcheck completeness: an unbuilt in-scope Consumer object makes `Architecture_DomainBoundaries` exit 2), only its ctest entry is `DISABLED` (T040 wording adjusted); (b) the `ShadeopAbi_Jit_*` shader compiles, `--jit` included, run at ctest time inside `check_jit_parity.cmake`, not as build steps (T044 wording adjusted).
5. **`CVariable::entry`: GitHub #140 (P1)**, sentence in 1.7 corrected. The `entry=-` reference is kept.
6. `reference/if_branches.txt` is left as is (the maintainer is checking the evaluator quirks).
7. **`ShadeopAbi_Jit_PerThread8` flakiness risk** (8 distinct per-thread pointers on a 2-core Linux host) noted at T115 in tasks.md; no change to the test.

### 1.9 Checkpoint 1 (T050, T051), after the T049 approval

**macOS** (`cpujob 038-p1-all ctest --test-dir build -LE determinism-sweep -j4 --output-on-failure`, log `build/p1-all.log`, 205 s): 1073 tests (1052 of Phase 0 plus the 21 added), 16 failed, the other 1057 passed or are `Not Run (Disabled)` (4: `Preview_SelfRegistration`, `SceneInterface_ContextInstallRules`, `SceneInterface_Services`, `LibShader_CompilerTableDrift`). The 16 failures are exactly the expected red set: the 8 `or038_red` checks, `SceneInterface_ContextInstall` and the 7 `ShadeopAbi_Jit_*`. Phase 0 baseline failures on macOS: none, and none appeared. `ctest -L architecture` (`038-p1-arch`): 21/21 passed (`ctest -N -L architecture` lists exactly 21). `ctest -L or038_red` (`038-pN-arch38`): 8 tests, all failing, the expected set (`Preview_NoCoreReach`, `Precomp_NoHiderCore`, `SceneInterface_RibOutNoCore`, `_RibVectorResidual`, `_NoCoreInclude`, `_HooksAndRecords`, `_ActiveContextAssignments`, `Compiler_IncludeBoundary`).

**Linux** (tree `openrender-038`, synced with `rsync` and `sha256sum -c` OK; `cpujob 038-p1-lx-build` exit 0 with 0 `warning:`; `038-p1-lx-test ctest -LE determinism-sweep -j2`, 680 s; logs `lx-cp-*.log` on the remote): 1074 tests, 23 failed, the other 1051 passed or are `Not Run (Disabled)` (the same 4). The 23 are the 16 expected red tests above plus the 7 Phase 0 baseline failures, unchanged and named there: #44 `LibShader_McompConsistency`, #45 `TextureTile_RenderByteIdentical`, #46 `TextureTile_CacheByteIdentical`, `_CacheDefaultDirectory`, `_CacheStaleDetection`, `_CacheConcurrentWrite`, #57 `CoverageAnalytic_alpha-sides2-tjunction-r4_reyes`. `ctest -L architecture`: 21/21 passed. `ctest -L or038_red`: the same 8 red checks.

Nothing is left unexplained: no new failure outside the expected red set on either host.

## 2. Phase 2: services, declaration table, hooks and records

### 2.1 What landed (T054 to T065)

- **T054** `src/ri/state/declarations.{h,cpp}`: `CDeclarations` (reference counted `acquire`/`release`/`active`, `declare`, `find`, `numGlobals`/`global`, `setGlobalObserver`). The body of the standard set and of `declare` is `CRenderer::initDeclarations()` / `declareVariable()` moved without a change of order or rule (the 121-variable list and its 29 globals are what the reference pins). Two members beyond the contract: `makeGlobal(CVariable *)` (the display code makes an output variable global: `rendererDisplay.cpp:634,658` called `CRenderer::makeGlobalVariable`) and `variableList()` (the head of the variable list, so a program can walk it). Depends on Infra and Graphics State only (`archcheck`: 0 violations).
- **T055** `src/ri/parse/delayedData.h` (`CDelayedData` moved verbatim; includes `common/global.h`, `common/algebra.h` for `vector` and `<stdlib.h>` for `free`); the class is gone from `geometry/delayed.h`.
- **T056** `parse/riCondition.h`. **T057** `parse/riServices.{h,cpp}`: the services, `riRetrieveVariable`, `riLocateFile`, `CRiStandardSession`; the standard file search is `CRenderer::locateFile` without the network or the temporary directory, with the working-directory fallback and the core's `info` texts.
- **T058** the core on `CDeclarations`: `rendererDeclarations.cpp` (`initDeclarations` acquires the table, installs the observer, keeps the coordinate systems and the global identifiers, installs `CCoreDeclarationService`/`CCoreFileSearchService` keeping the previous ones; `shutdownDeclarations` reverses; `declareVariable`/`retrieveVariable` are out-of-line forwarders), `rendererDeclarationsDispatch.cpp` (`makeGlobalVariable` is now `notifyGlobalVariable`, the context loop only) and its stub (no-op), `renderer.h` (the three statics out), `rendererStatics.cpp` (the three definitions out, the stale `RiBeginLite` sentence reworded), `rendererServicesImpl.h`. Also `rendererDisplay.cpp` (two calls become `CDeclarations::makeGlobal`, one include), not named by T058.
- **T059** `git mv src/ri/render/riHooks.h src/ri/parse/riHooks.h` (the file is unchanged: its comment never named its location; the includers use the bare name).
- **T060** `ifexpr.y`: `riEvaluateCondition(CRiInterface *, const char *)`, null `getAttributes(TRUE)`/`getOptions()` answers are "not found", no core include; `CRendererContext::ifParse` and its declaration deleted; `RiIfBeginV`/`RiElseIfV` call the new function.
- **T061** `rib.y`: `riRetrieveVariable`, the dead `if (FALSE)` branch deleted, core includes dropped; `rib.l`: `riLocateFile`, and `renderMan ? renderMan->getOptions() : NULL`.
- **T062** `ri.cpp`: the writer bootstrap is `if (ribOutMode) { CRiStandardSession session; globalArena.init(); ribParse(rcPath, NULL); globalArena.tini(); } else { ribParse(rcPath, NULL); }` (conditional on `ribOutMode`, as approved at T053); `delayedData.h`, `riServices.h`, `common/containers.h` and `common/memory.h` included (they came through `delayed.h` and `object.h` before).
- **T063** `rendererProcedures.cpp` uses `delayedData.h` (it used nothing else of `delayed.h`); the comment of `procDynamicLoadHook.h` names the new header.
- **T064** `src/ri/CMakeLists.txt`: `ribOut_sources` loses the six Core files; gains `parse/riServices.cpp`, `state/declarations.cpp`, `ifexpr_generated.cpp` and, **not foreseen by tasks.md**, `state/attributes.cpp`, `state/options.cpp` and `state/shaderParameterHooks.cpp`: `ifexpr_generated.cpp` calls `CAttributes::find` and `COptions::find`, and `libRIBOut` did not link without them (they are Graphics State files, so the dependency table allows them). `riServices.cpp` and `declarations.cpp` are in `ri_sources` (so in `ribRender` and `ribVector`); `ifexpr_generated.cpp` is compiled into all three.
- **T065** the approved edits E1 to E5 and E7 were applied: `test_procedural_operand.cpp` (`delayedData.h` added after `delayed.h`), `test_object_ids.cpp`, `SceneInterface_Services` enabled, the three labels moved to `architecture`, the comment of `test_self_registration.cpp`. No `riHooks.h` include line was needed (E3).

`SceneInterface_Services` passed on its first run, with no edit to the test; `SceneInterface_WriterText` and `_IfCharacterization` pass unchanged against the Phase 1 references (macOS and Linux).

### 2.2 Architecture (T067)

`ctest -L architecture` first failed with exactly the 10 DB-015 lines reported `STALE` (0 violations, 0 new cycles, 0 `badid`), as predicted. Then `tests/architecture/allowlist.txt:297-308` was deleted (the header, the 10 lines and the blank line after them; no line added) and `metrics-baseline.txt` set from archcheck's report: `SceneInterface 0 0`, `TOTAL 606 90` (606/90, not the expected 606/91: the figure is archcheck's, not typed from the plan). `ctest -L architecture` then: 24/24 passed (the 21 and the 3 moved checks), no `STALE`. `ctest -L or038_red`: 5 tests, all failing, the later phases' checks (`SceneInterface_RibVectorResidual`, `_ActiveContextAssignments`, `Preview_NoCoreReach`, `Compiler_IncludeBoundary`, `Precomp_NoHiderCore`).

### 2.3 Departures from tasks.md and the plan, approved at the Phase 2 stop (2026-11-02)

1. **`capture_declared_set.cpp` edited** (an approved Phase 1 program that read the statics T058 removes; it would not compile, macOS and Linux). Exactly: `sitest::declaredSetText(CDeclarations::variableList(), CDeclarations::numGlobals(), [](int i) { return static_cast<const CVariable *>(CDeclarations::global(i)); })` and `#include "declarations.h"` after `#include "common/memory.h"`; nothing else. Check: the edited program, run (`cpujob 038-p2-capcheck`) into a scratch directory, writes a `declared_set.txt` byte-identical to `reference/declared_set.txt` (`cmp`), which also shows the move left the declared set, its order and the global numbering unchanged.
2. **`rendererDisplay.cpp`** (not named by T058): its two `makeGlobalVariable` calls (`:634`, `:658`) became `CDeclarations::makeGlobal`, one include added.
3. **Two `CDeclarations` members beyond contracts/scene-services.md:** `makeGlobal(CVariable *)` (the display code makes an output variable global after it was declared) and `variableList()` (the head of the variable list, for the capture program and for tests that walk it).
4. **`libRIBOut` gains `state/attributes.cpp`, `state/options.cpp` and `state/shaderParameterHooks.cpp`**: `ifexpr_generated.cpp`, compiled into `ribOut` by decision Q6, calls `CAttributes::find` and `COptions::find`. They are Graphics State files, so Principle VIII allows them, and their process-wide state is duplicated across `libRIBOut` and `libRIBRender` the same way `channelRegistry`, `variableLookup` and `displayChannel` already are.
5. **`TOTAL 606 90` instead of the expected `606 91`.** The plan counted `initDeclarations`, `shutdownDeclarations` and `ifParse` as the members that leave the measured set and assumed `CRenderer::context` stays referenced by another measured class. It does not: its only measured reference was the dead `if (FALSE)` branch of `rib.y` that T061 deletes (`delayed.cpp` names it only in comments). So 4 members leave (94 to 90), and 6 references (612 to 606). `retrieveVariable` and `locateFile` stay referenced by `blobby.cpp` and `blobbyRepeller.cpp` (DB-004). The figure is archcheck's report, as approved; the build passes with it.

### 2.4 Checkpoint 2 (T066, T069, T070, T071), clean build

**macOS** (`cpujob 038-p2-build cmake --build build --config Release -j4`: exit 0, no `warning:`/`error:`; `038-p2-all ctest -LE determinism-sweep -j4`, 182 s): 1077 tests, 3 `Not Run (Disabled)` (`Preview_SelfRegistration`, `SceneInterface_ContextInstallRules`, `LibShader_CompilerTableDrift`), 13 failed, all expected: the 5 `or038_red` checks (`Preview_NoCoreReach`, `Precomp_NoHiderCore`, `SceneInterface_RibVectorResidual`, `_ActiveContextAssignments`, `Compiler_IncludeBoundary`), `SceneInterface_ContextInstall` and the 7 `ShadeopAbi_Jit_*`. That is the Phase 1 red set minus the three checks that moved to `architecture`, with `SceneInterface_Services` green. No baseline failure on macOS. `ctest -L architecture`: 24/24 (`ctest -N -L architecture` lists 24), archcheck `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`, `SceneInterface refs 0 (baseline 0)`, `TOTAL refs 606 (baseline 606) distinct 90 (baseline 90)`. `ctest -L or038_red`: the 5 above, all failing.

**Linux** (the edited file synced with `rsync -c` and `sha256sum -c`; `cpujob 038-p2-lx-build cmake --build build -j2`: exit 0, 0 `warning:`; `ctest -LE determinism-sweep -j2`, 688 s): 1078 tests, 3 `Not Run (Disabled)`, 20 failed: the same 13 plus the 7 Phase 0 baseline failures (#44 `LibShader_McompConsistency`, #45 `TextureTile_RenderByteIdentical`, #46 the four `TextureTile_Cache*`, #57 `CoverageAnalytic_alpha-sides2-tjunction-r4_reyes`). `ctest -L architecture`: 24/24. `ctest -L or038_red`: the same 5. Logs on the remote: `lx-p2b-*.log`.

## 3. Phase 3: explicit active context

(Not started.)

## 4. Phase 4: preview back-end, data viewer, `ribVector`

(Not started.)

## 5. Phase 5: standalone compiler tables

(Not started.)

## 6. Phase 6: JIT DSO call

(Not started.)

## 7. Phase 7: `precomp`

(Not started.)

## 8. Phase 8: documentation

(Not started.)

## 9. Phase 9: polish

(Not started.)
