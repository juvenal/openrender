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
- `cpujob 038-pN-arch38 ctest -L architecture_038`: "No tests were found" (the label is not yet used; expected at Phase 0).

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
- `cpujob 038-p0-lx-arch ctest -L architecture`: 21/21 passed. archcheck: `platform=linux objects=884 sources=203 edges=2549 (include=1644 link=905)`, `PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`, `SceneInterface 6/6`, `TOTAL 612/94`. `-L architecture_038`: no tests.

### 0.9 Summary

TEST-APPROVAL (Phase 0, T010): no approved test, check or reference was edited or added. Files this phase changed: the spec documents (D4-a and the line-number corrections of 0.2 and 0.5) and this file. No count in the documents differed from the measurement. Nothing else is open from Phase 0; the maintainer commits the staged documents.

## 1. Phase 1: tests first, references, Linux gate

(Not started.)

## 2. Phase 2: services, declaration table, hooks and records

(Not started.)

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
