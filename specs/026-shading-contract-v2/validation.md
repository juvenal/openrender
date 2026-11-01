# Validation record: Shading Contract v2

**Spec**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md) | **Tasks**: [tasks.md](tasks.md)

Seeded at the tasks stage (2026-10-31) with §4, the finding triage. Every other section is filled in by the
implement stage, phase by phase. Nothing below the headings of §0 to §3 and §5 has been measured yet.

## §0 Starting-tree measurements (Phase 1)

**T001 orientation.** Branch `026-shading-contract-v2`, HEAD `0475e1e4` (spec commit), base `d01da47` (= master `d01da474`, unchanged; `git merge-base HEAD master` = `d01da47`). Only `specs/026-shading-contract-v2/` was staged. `.specify/feature.json` = `specs/026-shading-contract-v2`. `git check-ignore build build-red build-nojit build-base build-logs` prints all five. Disk at start: 9.3 GiB free (`df -h /Volumes/Projects`); 8.5 GiB after the macOS build, both baseline suites and the sweeps. The "Spec-kit agent rules" section of `CLAUDE.md` is byte-identical in the worktree copy and the main checkout's copy (`cmp` identical, commit time 1793332971 for both, main checkout has no uncommitted change to it); the worktree copy was followed.

**T004 macOS baseline** (starting `src/`, no source edit; Release, JIT ON, vcpkg from `VCPKG_ROOT=/Users/juvenal/.vcpkg`, `VCPKG_INSTALLED_DIR=/Users/juvenal/.cache/vcpkg-installed/openrender`): configure clean, build exit 0, `ctest -LE determinism-sweep -j4`: **929 tests, 100% passed** (243.75 s). One test skips itself on the baseline: `LibShader_Format_rslo_lower_minor` (Skipped). No test is Disabled/Not Run. Manifest: 819 images (`baseline-images-macos.sha256`), 389 test entries in `baseline-diffs-macos.txt`. Logs: `tools/base-LastTest-macos.log`.

**T005 noise check**: the suite run a second time: `image_manifest.sh diff` reports 819 images, 0 differing, 0 missing; every `MaxBlockAvgDiff` equal (`diff` of the two `diffs` outputs empty). `baseline-image-noise-macos.txt` is therefore empty (a comment line only). (Spec 028 found two scratch files; none exists here.)

**T006 architecture and imports**: `tools/archcheck-base-macos.txt`: `ALLOWED DB-001 x 8 DB-004 x 52 DB-005 x 8 DB-006 x 33 DB-010 x 8 DB-011 x 166 DB-015 x 10 DB-017 x 78 DB-019 x 3 DB-021 x 3 DB-025 x 12`; `METRIC Shading refs 0 (baseline 0)`, TOTAL 613; `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`. The 6 imports of research R10 (`tools/libshader-imports-base-macos.txt`, all "dynamically looked up"): `CPhotonMap::lookup(float*, float const*, int)`, `CGatherBundle::CGatherBundle()`, `CShadingContext::trace(CRay*)`, `CShadingContext::traceEx(CRayBundle*)`, vtable for `CTraceBundle`, vtable for `CTransmissionBundle`. `dyld_info -fixups` (`libshader-fixups-base-macos.txt`, 39,589 lines) shows chained fixups: the flat-namespace imports are `bind` entries in `__DATA_CONST,__got`, resolved at load (e.g. `bind <flat-namespace>/vtable for CTraceBundle`), not lazy.

**T007 determinism sweep, starting tree** (SC-007 for the sweep, B1): `ctest -L '^determinism-sweep$' -j2`: **419 tests, 100% passed** (587 s). The sweep leaves images under `build/tests/visual/determinism-sweep/<scene>/<scene>.tif`: `baseline-sweep-images-macos.sha256` holds **419 images** (the tasks text says 413 scenes; the tree has 419 tests and 419 images, recorded as measured). A second run: 419 images, 0 differing, 0 missing; `baseline-sweep-noise-macos.txt` is empty (comment only). Log: `tools/base-sweep-LastTest-macos.log`. The final comparison (T176) can therefore diff images.

**T008 static counts** (this worktree, `d01da47`; sites by symbol, not number):

| Count | Measured | Plan | Design decision changed? |
|---|---|---|---|
| pure virtuals of `CRendererServices` (`rendererServices.h`, `= 0;`) | 57 | 57 | no |
| `override`s in `src/ri/render/rendererServicesImpl.h` | 57 | 57 | no |
| `findCoordinateSystem(` / `jitFindCoordinateSystem(` call sites outside declarations/definitions/comments | 25: `shaderFunctions.h` 17 (`:511, 531-532, 551, 571-572, 598, 615, 632, 652-653, 675-676, 709, 732, 2166, 2267`), `giFunctions.h` 1 (`:195`), `shaderOpcodes.h` 1 (`:513`), `shading.cpp` 3 (`:3532, :3644, :3684`), `rslOps.cpp` 3 (`:966, :981, :1258`, through `jitFindCoordinateSystem`); plus the 2 forwarders in `shading.cpp` (`:2846`, `:3003`) | 25 + 2 | no |
| Shading trace call sites | 10: `executeMisc.cpp` `trace(cRay)` at `:793` (transmission/visibility probe) and `:939` (reflection probe), `traceEx(` at `:805, :823` (`traceTransmission`) and `:954, :971, :990, :999` (`traceReflection`); `shading.cpp` `traceEx(lastGather)` at `:4487, :4498` | 10 | no |
| trace call sites in `irradianceDispatch.cpp` | 2 (`:371` occlusion branch, `:471` indirectdiffuse branch) | 2 | no |
| `currentObject->attributes` in `src/libshader` | 23 textual, 22 in code (1 is a comment, `shading.h:819`): `shading.cpp` 15, `execute.cpp` 2, `executeMisc.cpp` 2, `giFunctions.h` 2, `shaderFunctions.h` 1 | "17" (a figure to re-measure) | no (not a stop condition) |
| of those, compiled into `execute.cpp`/`executeMisc.cpp`/`shaderFunctions.h`/`giFunctions.h` | 7 reads (`execute.cpp:308, :385`, `executeMisc.cpp:720, :846`, `giFunctions.h:291, :338`, `shaderFunctions.h:364`) + 1 `setOffendingObject` (`shading.cpp:585`, `scripterror`) | 7 + 1 | no |
| allowlist keys to be deleted | present: DB-006 `execute.cpp -> src/ri/geometry/surface.h` (`allowlist.txt:88`), `executeMisc.cpp -> src/ri/geometry/surface.h` (`:89`); DB-001 `executeMisc.cpp -> _ZN15CShadingContext5traceEP4CRay` (`:15`), `executeMisc.cpp -> _ZN15CShadingContext7traceExEP10CRayBundle` (`:16`), `shading.cpp -> _ZN15CShadingContext7traceExEP10CRayBundle` (`:21`); `shading.cpp -> src/ri/geometry/object.h` (`:90`) stays | 5 + 1 kept | no |

**T009 hit-mode values the visual suite uses**: none. `grep -rE 'hitmode'` over every `.rib` and test source in the repo (excluding `build`, `specs`) finds no `camerahitmode`, `specularhitmode`, `transmissionhitmode` or `diffusehitmode` in any scene or test; the only mentions are the token definitions (`riTokens.cpp`), the parser (`attributes.cpp`) and the documentation (`docs/`, `doc/`). Every visual scene runs with the defaults (transmission and diffuse `"primitive"`, specular and camera `"shader"`). Phase 2's `Situations` scenes therefore cover (call site, default hit mode) pairs; the non-default modes are covered by `Situations` scenes of their own (research R13), not by the visual suite.

**T010 `linux_sync.sh --dry-run`**: 3,110 files listed, none under `build*` or `.git/` (the two `.git*` entries are `.github` and `.gitignore`); no Linux-only file is listed; the script refuses a remote root other than `Projects/Development/CLI/openrender-026` and any root ending in `/openrender`, `/openrender-039` or `/openrender-037`. `rsync -rlpc --from0 --files-from` works with the local openrsync (protocol 29); the remote rsync is 3.2.7 (first real push at T035).

**T043 checkpoint (approved 2026-10-31, maintainer via the coordinator, all recommendations):** `replay/macos.txt` (including its three sections re-recorded after the D1 fix: `transmission`, `visibility`, `shadow`), `replay/linux.txt`, the rewritten `replay_shadow.sl`/`replay_shadow.rib`, both image noise lists, both sweep manifests with their noise lists, and the suite manifests and diffs files for both platforms. The replay files are never re-recorded from here on. D4 is GitHub #123 (P0, Bug Tracker).

**T016 TEST-APPROVAL (answered 2026-10-31, maintainer via the coordinator): approved, every recommendation accepted** (replay design, the eight scenes, the hook shape with the public `reportTraceRequest` member and the null guard at the 12 sites, the eight mutations with a clean `git diff` after each revert). **Carried to Phase 2: the `Situations` scene set must cover the non-default hit modes itself, because T009 found none in the visual suite.** Nothing recorded in Phase 1 counts as an approved reference before T043.

**Tooling notes.** The shell prints `setValueForKeyFakeAssocArray`/`valueForKeyFakeAssocArray` errors on every command (filtered). The Linux host `juvenal@ubuntu-2404-desktop.v2-labs.io` answers (2 cores, 11 GB free in `/home`, cmake 3.28.3, GCC 13.3.0, `~/.claude/bin/cpujob` present); `openrender` and `openrender-037` trees exist there and are not touched.

## §1 Checkpoint records (one table per phase)

### Phase 1 (macOS; the hook is in the tree)

| Check | Result |
|---|---|
| CP-a build (`cpujob 026-p1-build`, `-j4`, Release, `-Werror`) | exit 0; no compiler warning; `ribVector` and the two replay executables built (the only warnings are the pre-existing `ld`/Swift ones, 21 against 23 on the starting tree) |
| CP-b suite (`-LE determinism-sweep -j4`), hook in, replay tests still disabled (T022) | 929 tests, 100% passed; the 9 `ShadingContract_*` tests "Not Run (Disabled)"; `LibShader_Format_rslo_lower_minor` skips itself as on the baseline |
| CP-d (T022) | 819 images, **0 differing, 0 missing**; every `MaxBlockAvgDiff` equal to `baseline-diffs-macos.txt` (the hook adds no output) |
| Record (T023) | `ShadingContract_TraceReplayRecord` run twice: `replay-fresh.txt` byte-identical (788 lines). Copied to `tests/shading_contract/replay/macos.txt` |
| Requests per family and backend (macOS) | `trace` 10 requests / 28 rays; `transmission` 2 / 20; `visibility` 8 / 8; `gather` 12 / 48; `environment` 3 / 24; `shadow` 2 / 20; `occlusion` 48 / 48; `indirectdiffuse` 48 / 48, **identical for `.rslo` and `.slo`** (every section of the two backends is byte-identical on macOS). No family is empty |
| T024 | the 8 `TraceReplay_*` enabled: 8 of 8 green by construction |
| T025 to T032 | eight mutations, each turned its named test red (§2) |
| T033: after the reverts | `git diff` empty, full rebuild, 8 of 8 green; CP-b again (`p1b.LastTest.log`): **938 tests, 100% passed** (929 + the 9 now enabled), CP-d again **0 differing, 0 missing, `MaxBlockAvgDiff` equal** |
| CP-c (T034) `ctest -L architecture` | 21 of 21 passed |
| CP-c archcheck report (`build-logs/archcheck-p1.txt`) | `ALLOWED DB-001 x 8 DB-004 x 52 DB-005 x 8 DB-006 x 33 DB-010 x 8 DB-011 x 166 DB-015 x 10 DB-017 x 78 DB-019 x 3 DB-021 x 3 DB-025 x 12`; `METRIC Shading refs 0 (baseline 0)`, TOTAL 613 (baseline 613); `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`. The `ALLOWED`/`METRIC`/`RESULT` lines are identical to the starting tree's. No `incomplete build tree` with the two replay executables built |
| Imports | the 6 flat-namespace imports are unchanged (`nm` diff against `libshader-imports-base-macos.txt` empty) |
| After the Linux-driven test-header fix (below) | rebuild clean, 8 of 8 replay tests green again |

The only edit made to approved test code since T016 is the GCC 13 fix in `tests/shading_contract/replay/trace_replay.h` (§3): `backends()` returns a `static const` vector built from one initializer list instead of `push_back` after a one-element list. It changes no value and no recorded byte.


### Phase 2 (tests first; no `src/` edit): macOS, option OFF

| Check | Result |
|---|---|
| `git diff d920c784 -- src` | the only `src/` change since Phase 1 is the D1 fix (`PARAMETER_N` on four builtins, §4.4); Phase 2 itself edits no `src/` file |
| T063 CP-a (`cpujob 026-p2-build`, `-j4`, `-Werror`, option OFF) | exit 0; the 27 contract-test shaders compile as `.rslo` and `.slo` (build steps, now part of the default build through the replay executables' dependency) |
| T063 CP-c `ctest -L architecture` | 21 of 21; archcheck: `ALLOWED DB-001 x 8 DB-004 x 52 DB-005 x 8 DB-006 x 33 ...`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0` (identical to the baseline) |
| `ctest -N -L '^shading_contract$'` | 36 tests: the 8 replay tests (enabled) and **28 new tests, every one "Disabled"** |
| `ctest -L '^shading_contract$'` | the 8 replay tests pass; the 28 are "Not Run (Disabled)" |
| CP-b | not repeated (T063): nothing the suite runs has changed |

### Phase 2 approval and the amendment (T064, 2026-10-31)

The 28 Phase 2 tests were approved as written. One design amendment was decided with them, about the standalone program's current object:

- The trace builtins read `currentObject->attributes` (`execute.cpp` `plBegin`, `executeMisc.cpp:720` and `:846`, `shading.cpp` `jitParameterList`), so with the null object of the standalone program (QE) they would crash, and T129 as first written (`objectAttributes()` null without an object) would not have fixed that. Phase 2 therefore used a harness stand-in (`attachFakeObject()`) for `TraceRequests` and `NoServices_Tracer` only.
- Amended (T064), then refined at T066 (2026-10-31, the maintainer's decision): `objectAttributes()` stays null when there is no object (T129); its consumers on the trace path (`plBegin`, `jitParameterList`, and the flag reads of `traceTransmission` and `traceReflection`) take built-in defaults when it is null (T129, T130), so no `static CAttributes` goes into `libshader_shading`; and `attachFakeObject()` is deleted in Phase 8 (T139a, an approved edit to approved test code, diff shown in the Phase 8 report). A read that cannot be made null-tolerant keeps the stand-in for itself and is named.
- The first version of the amendment returned a default `CAttributes` set; it was dropped before any code was written because constructing one in `libshader_shading` can add imports (`CAttributes::CAttributes()`, its vtable) that SC-011 forbids. The import measurement stays at the Phase 7 checkpoint as a check, expecting no growth.

### Phase 2: Linux (`openrender-026`, GCC 13.3, `-Werror`, `-j2`)

| Check | Result |
|---|---|
| push / verify | 3,204 files, 0 differences |
| build | exit 0, no warning |
| `ctest -L architecture` | 21 of 21; `ALLOWED` lines identical, `RESULT PASS` |
| `ctest -L '^shading_contract$'` | the 8 replay tests pass; the 28 new tests "Not Run (Disabled)" |
| GCC 13 `-fsyntax-only -Wall -Wextra -Wpedantic -Wformat-security -Werror` of all 17 new sources against a scratch overlay of the contract headers | clean (no output). This is the check that the option-ON programs, which only build from Phase 3 on, compile under GCC; the overlay holds `serviceTypes.h`, `tracer.h`, `shaderGlobals.h`, `situation.h`, `sloLoader.h` and a copy of `rendererServices.h` and `shading.h` with the v2 members written exactly as the contracts say (scratch, outside the tree; not staged) |

### Phase 3 (the foundation: headers, version, stub, loader move): macOS

The two test-code fixes of `tools/phase3-test-fixes.diff` (D5, D6) were confirmed by the maintainer on 2026-10-31 and are applied; the four Phase 3 tests are ENABLED. Rows marked "before confirmation" were measured on a trial tree with the diff applied, before it was confirmed; the final-tree rerun is in the last row.

| Check | Result |
|---|---|
| T075 configure | `build` reconfigured with `-UOPENRENDER_SHADING_CONTRACT_TESTS` (a cache entry written as OFF by the earlier configure survives a changed default) |
| T075 compile of both contract programs against the real headers | no error, no warning, **no compile fix needed** (the headers match the contract text) |
| T076 link line | `libshader_shading` and `libRIBCommon` only; `StandaloneLinkLine` passes |
| T076 stub | the standalone program defines the 6 imports itself (`CPhotonMap::lookup`, `CGatherBundle::CGatherBundle()`, the vtables of `CTraceBundle` and `CTransmissionBundle`, `CShadingContext::trace`, `traceEx`); 0 flat-namespace fixups; the table equals the measured imports |
| T076 `libshader_shading` imports | `diff` against `tools/libshader-imports-base-macos.txt` is **empty** (SC-011 holds: the moved `.slo` loader and the new headers add none). Saved as `tools/libshader-imports-phase3-macos.txt` |
| T077, before confirmation | `StandaloneLink` (both `.rslo` and `.slo`), `StandaloneLinkLine`, `ShaderGlobalsDouble`, `DoubleGlobalsMatchRenderer`: 4 of 4 pass |
| T077, without the fixes | Without the fixes, `StandaloneLink` fails (D5) and `ShaderGlobalsDouble` fails 32 of 399 checks in Release (D6) |
| CP-a, with fixes | full build clean |
| CP-b, with fixes | 942 tests, 100% passed |
| CP-c | `ctest -L architecture` 21 of 21; `ALLOWED` DB-001 x 8, DB-005 x 8, DB-006 x 33, Shading metric 0, `RESULT PASS` (identical to the baseline) |
| CP-d, with fixes | 819 images, 0 differing, 0 missing, every `MaxBlockAvgDiff` equal. One INFO line: a stray `ri.tif` in the working directory of `DoubleGlobalsMatchRenderer` (the rendered harness's `RiEnd` writes it); harmless, to be pointed at the test's scratch directory in Phase 8 |
| CP-m, with fixes | the 8 replay tests and the 4 new ones pass; the other 24 contract tests "Not Run (Disabled)" |
| includes of the new files | `serviceTypes.h`, `tracer.h`, `shaderGlobals.h`, `situation.{h,cpp}`, `sloLoader.{h,cpp}` include only Shading or Infra headers (no `renderer*.h`, nothing under `src/ri/{render,hiders,geometry}`) |
| Final tree (diff applied, 4 tests ENABLED), 2026-10-31 | full build clean (`-Werror`, no warning); `ctest -L shading_contract`: the 8 replay tests, `StandaloneLinkLine` and the 4 new tests pass, 24 "Not Run (Disabled)"; CP-b `-LE determinism-sweep` **942 tests, 100% passed**; CP-d (fresh stamp) **819 images, 0 differing, 0 missing**, `MaxBlockAvgDiff` equal to `baseline-diffs-macos.txt`, 1 INFO (the stray `ri.tif`); CP-c 21 of 21, `RESULT PASS` |
| T079 JIT-off compile (`build-nojit`, `-DOPENRENDER_ENABLE_JIT=OFF`) | configure and `libshader_shading` build exit 0, no error or warning under `-Werror`; `sloLoader.cpp` compiles empty and the library exports no `parseSloShader`. `build-nojit` is kept for T169 |

## §2 Red evidence (Test gates, mutations)

### Phase 1 (T015): the replay targets fail to build on the starting tree

`cpujob 026-p1-red cmake --build build --target test_shading_contract_trace_replay_record test_shading_contract_trace_replay_check -j4` (an `EXCLUDE_FROM_ALL` target builds when named with `--target`, so the marker did not have to be removed; the replay shaders built first, as dependencies). Exit 2; log `build-logs/026-p1-red.log`. Every error is the missing hook, none is anything else:

```
tests/shading_contract/replay/trace_replay.h:75:14: error: use of undeclared identifier 'TRACEREQUEST_TRACE'
tests/shading_contract/replay/trace_replay.h:76:14: error: use of undeclared identifier 'TRACEREQUEST_TRANSMISSION'
tests/shading_contract/replay/trace_replay.h:77:14: error: use of undeclared identifier 'TRACEREQUEST_VISIBILITY'
tests/shading_contract/replay/trace_replay.h:78:14: error: use of undeclared identifier 'TRACEREQUEST_GATHER'
tests/shading_contract/replay/trace_replay.h:79:14: error: use of undeclared identifier 'TRACEREQUEST_OCCLUSION'
tests/shading_contract/replay/trace_replay.h:80:14: error: use of undeclared identifier 'TRACEREQUEST_INDIRECTDIFFUSE'
tests/shading_contract/replay/trace_replay.h:111:31: error: unknown type name 'CTraceRequestReport'
tests/shading_contract/replay/trace_replay.h:154:26: error: no member named 'traceRequestObserver' in 'CShadingContext'
tests/shading_contract/replay/trace_replay.h:156:26: error: no member named 'traceRequestObserver' in 'CShadingContext'
9 errors generated.
```

Scene sanity (not a test): the 8 scenes rendered with the real `orender` on both backends (16 renders, no diagnostic) before the hook existed, so a scene problem cannot be mistaken for a hook problem later.

### Phase 2 (T062): the new tests fail to build on the starting tree

`cpujob 026-red-configure cmake -B build-red -S . -DCMAKE_BUILD_TYPE=Release -DOPENRENDER_SHADING_CONTRACT_TESTS=ON`, then `cpujob 026-red cmake --build build-red --target test_shading_contract_standalone test_shading_contract_rendered test_shading_contract_situations -j4 -- -k` (`-k`, so every translation unit reports; the situations program is built too). Exit 2; log `build-logs/026-red.log`; `build-red` removed afterwards. 13 translation units fail and every failure is a missing contract header:

```
standalone: case_attributes.cpp, case_link.cpp, case_matrices.cpp, case_messages.cpp, case_no_services.cpp, case_trace_requests.cpp
            tests/shading_contract/contract_services.h:44:10: fatal error: 'libshader/shading/serviceTypes.h' file not found
standalone: case_globals_double.cpp
            tests/shading_contract/case_globals_double.cpp:28:10: fatal error: 'libshader/shading/shaderGlobals.h' file not found
rendered:   case_canned_trace.cpp, case_double_globals.cpp, case_gather_requests.cpp, case_no_tracer.cpp, case_real_answers.cpp
            tests/shading_contract/contract_services.h:44:10: fatal error: 'libshader/shading/serviceTypes.h' file not found
situations: case_situations.cpp
            tests/shading_contract/case_situations.cpp:49:10: fatal error: 'libshader/shading/situation.h' file not found
```

`tracer.h` and `sloLoader.h` are named by later `#include`s in the same translation units, which the first fatal error hides; they are missing too (the overlay check, §1, shows they are the only other missing pieces). The programs that need nothing new (`standalone_main.cpp`, `rendered_main.cpp`, `situations_main.cpp`, `load_stub.cpp`) compile, which shows the load stub is consistent with the starting tree.

Tests registered: 28, all `DISABLED` (list in the T064 report).

### Phase 1 (T025 to T032): the eight replay mutations (macOS)

Each mutation: edited, `cmake --build build --target test_shading_contract_trace_replay_check -j4`, `ctest -L '^shading_contract$'`, reverted with `git checkout -- <file>` (the hook is staged, so that restores the hooked file), `git diff` empty after the revert (the first row printed 27 lines only because `tests/shading_contract/CMakeLists.txt` had an unstaged `DISABLED` removal at that moment; staged since, every later row is 0 lines). Driver and logs: `build-logs/mut-<name>-{build,test}.log`, `build-logs/mut-<name>.diff`. Every named test went red, with the first differing field named:

| Builtin | Mutation | Named test | First failing line | Other tests red under the same edit |
|---|---|---|---|---|
| trace | `executeMisc.cpp`, `traceReflection`: `cRay->tmin = rays->bias;` -> `cRay->tmin = 0;` | `ShadingContract_TraceReplay_trace` RED | `TraceReplay trace (rslo): request 0 ray 0 field tmin differs: recorded tmin=0x1.47ae14p-7, fresh tmin=0x0p+0` | `environment` (it reaches `traceReflection` too) |
| transmission | `executeMisc.cpp`, `traceTransmission` (non-motion branch): `cRay->time = rays->time;` -> `cRay->time = rays->time + 0.25f;` (the task's "or change the copied time" variant: `traceTransmission` has one bundle, no exterior/interior pair to swap) | `ShadingContract_TraceReplay_transmission` RED | `TraceReplay transmission (rslo): request 0 ray 0 field time differs: recorded time=0x0p+0, fresh time=0x1p-2` | `visibility`, `shadow` (same shared line) |
| visibility | `executeMisc.cpp`, `traceTransmission` probe branch: added `cRay->tmin = 0;` before the hook | `ShadingContract_TraceReplay_visibility` RED | `TraceReplay visibility (rslo): request 0 ray 0 field tmin differs: recorded tmin=0x1.47ae14p-7, fresh tmin=0x0p+0` | none |
| gather | `shading.cpp`, `gatherBegin`: both `ray->da = ...` assignments -> `ray->da = 0.0f;` | `ShadingContract_TraceReplay_gather` RED | `TraceReplay gather (rslo): request 8 ray 0 field da differs: recorded da=0x1p+0, fresh da=0x0p+0` | none |
| environment | `shaderFunctions.h:1892` (the `ENVIRONMENTEXPR_PRE` macro): `rays->bias = scratch->traceParams.bias;` -> `... * 2.0f;` | `ShadingContract_TraceReplay_environment` RED | `TraceReplay environment (rslo): request 0 ray 0 field tmin differs: recorded tmin=0x1.47ae14p-7, fresh tmin=0x1.47ae14p-6` | none |
| shadow | `shaderFunctions.h:2002` (the `SHADOWEXPR_PRE` macro): the same `* 2.0f` | `ShadingContract_TraceReplay_shadow` RED | `TraceReplay shadow (rslo): request 0 ray 0 field t differs: recorded t=0x1.6d530cp-3, fresh t=0x1.58d82cp-3` | none |
| occlusion | `irradianceDispatch.cpp:361` (occlusion branch of `CIrradianceCache::sample`): `ray.tmin = scratch->traceParams.bias;` -> `ray.tmin = 0;` | `ShadingContract_TraceReplay_occlusion` RED | `TraceReplay occlusion (rslo): request 0 ray 0 field tmin differs: recorded tmin=0x1.47ae14p-7, fresh tmin=0x0p+0` | none |
| indirectdiffuse | `irradianceDispatch.cpp:465` (indirectdiffuse branch): the same | `ShadingContract_TraceReplay_indirectdiffuse` RED | `TraceReplay indirectdiffuse (rslo): request 0 ray 0 field tmin differs: recorded tmin=0x1.47ae14p-7, fresh tmin=0x0p+0` | none |

The `gather` mutation is detected on request 8 (the second `gather()` call, whose cone is wide enough for a non-zero `da`); the first call's `da` is 0 in the recording. After the eight reverts the tree was rebuilt in full: `git diff` empty, 8 of 8 replay tests green, and CP-b/CP-d repeated (see §1).

### Phase 4 (T082): the getMatrix tests fail on the Phase 3 tree (macOS)

The five tests were enabled, and the three forwarding `getMatrix` overrides of T083 were added to `CForwardingServices`, `CRecordingServices` and `CTextureServices` (the only other edit; `src/` untouched). The build is clean; `ctest -R 'ShadingContract_(MatricesInterp|MatricesJit|NoServices_Matrix|RealAnswersInterp_Matrices|RealAnswersJit_Matrices)$'`: **0 of 5 passed**. Log: `build-logs/026-p4-red.log`.

| Test | Failures | Why (matches the expected red of T082) |
|---|---|---|
| `Matrices{Interp,Jit}` | 106 each | `svc.matrixCalls.size() == e.names.size()` x 15 (the double's `getMatrix` was never asked: Shading still calls the legacy `findCoordinateSystemWithType`, which the TEMPORARY block answers "false"); the transformed values x 76 (`Unknown coordinate system` -> identity); the `unknown` warning count x 14; `objectErrors` x 1 (the NoObject path) |
| `NoServices_Matrix` | 2 | `svc.count("getMatrix") == 2` and the `CODE_NOTSTARTED` once-warning count: with no services the old path still runs and warns `Unknown coordinate system` instead |
| `RealAnswersInterp_Matrices` | 14 | `fwd.matrixCalls.size() == row.names.size()`: the forwarder recorded no `getMatrix` |
| `RealAnswersJit_Matrices` | 23 | the same 14, plus 9 value mismatches |

### Phase 4 (T085 to T089): macOS, after the implementation and the D8 fix (T086a)

Implementation: `CRendererServicesImpl::getMatrix` (rendererShadingServices.cpp), `CShadingContext::findCoordinateSystem` rewritten over `getMatrix` with the diagnostics kept and a once-per-process `CODE_NOTSTARTED` warning for Unavailable or no services, and `findCoordinateSystem`/`findCoordinateSystemWithType` removed from the interface, the Impl and the six approved test overrides (T087). T086a (D8): the emitter's `ctransform` branch dispatches on the proto shape and calls the new `op_ctransform_ss`. `init.cpp` and the bind-time block of `rslOps.cpp` untouched.

| Check | Result |
|---|---|
| build | clean (no compiler warning; the linker warnings about duplicate LLVM libraries and the Swift package's macOS 13.0/13.3 versions are old) |
| T088 Phase 4 tests | all 5 **pass**: `MatricesInterp`, `MatricesJit`, `NoServices_Matrix`, `RealAnswersInterp_Matrices`, `RealAnswersJit_Matrices`. Before T086a the two JIT tests failed on `ctransform("rgb", "hsv", c)` only (D8) |
| CP-b `-LE determinism-sweep` | **947 tests, 100% passed** (`ShadingParity_*`, `ShadingServices_*`, `TextureSubstitute*` pass with only the approved wrapper edits) |
| CP-d (fresh stamp, after the `oshader` rebuild recompiled every shader) | 819 images, **0 differing, 0 missing**; `MaxBlockAvgDiff` equal to the baseline; 3 INFO lines (the `ri.tif` files the rendered contract tests leave in their cwd). No suite shader uses the two-space `ctransform` under `.slo` |
| CP-m | `ctest -L shading_contract`: 18 of 18 run pass; the rest "Not Run (Disabled)" |
| CP-c | `ctest -L architecture` 21 of 21; `ALLOWED` identical to the Phase 3 table (the same 11 DB lines), `RESULT PASS` |
| D7 count | `init.cpp` preprocessed with the build's flags (`-E -P`): **19** `CRenderer::findCoordinateSystem(` calls (= 17 `shaderFunctions.h` + 1 `giFunctions.h` + 1 `shaderOpcodes.h`: every macro site expands there), 3 other `findCoordinateSystem(` tokens (the declarations in `renderer.h` and `shading.h`), 1 `jitFindCoordinateSystem(` (its declaration). They stay |

### Phase 5 (T095): the getAttribute tests are red on the Phase 4 tree (macOS)

Enabled: `Attributes{Interp,Jit}`, `NoServices_Attribute`, `RealAnswers{Interp,Jit}_Attributes`, and the new `TypeMismatch_Attribute{Interp,Jit}`, `TypeMismatchReal_Attribute{Interp,Jit}`, `NullObject_Attributes{Interp,Jit}` and `MessageAccessors`; the three wrappers got their `getAttribute` forwarding override (T096). The default build is clean (the new sources compile). `ctest -R` of the 12 tests: **0 of 12 passed**. Logs: `build-logs/026-p5-red.log`, `026-p5-build-tm.log`.

| Test | Result | Why |
|---|---|---|
| `Attributes{Interp,Jit}`, `NoServices_Attribute` | SEGFAULT | `attribute()` dereferences `currentObject->attributes` and the standalone program has no object (QE); a crash is red evidence |
| `RealAnswers{Interp,Jit}_Attributes` | 3 failures each | `fwd.attributeCalls.size() == 6`: the forwarder recorded no `getAttribute` (the three rows) |
| `TypeMismatch_Attribute{Interp,Jit}` | SEGFAULT | the same null-object dereference; with an object the unsafe copy would overwrite the guards |
| `TypeMismatchReal_Attribute{Interp,Jit}` | Not Run | the executable fails to **build**: 15 errors, all `too many arguments to function call, expected 2, have 3` (`queryAttribute`/`queryOption`/`queryRendererInfo` take no `QueryType` yet) |
| `NullObject_Attributes{Interp,Jit}` | SEGFAULT | `attribute()` with `currentObject == nullptr` (F2) |
| `MessageAccessors` | Failed | all 8 `DEFFUNC` lines and both `jit*Parameter` members pass the other accessor (F4); shown green on a scratch copy with the constants swapped |

### Phase 5 (T098 to T107): the implementation, macOS and Linux

Implemented: `CRendererServicesImpl::getAttribute` (the three name tables moved verbatim from Shading, null object -> `NotFound`, the FR-026 type check), `CShadingContext::queryServiceAttribute` and the three members over it (a `QueryType` argument; one `CODE_BADTOKEN` warning per builtin kind per process for a mismatch, one `CODE_NOTSTARTED` for an unavailable service), `PARAMETEREXPR_PRE(accessor, expected)` with the 32 `DEFFUNC` lines updated and the F4 accessors swapped, `queryTypeOfResultKind`, the five message members and their JIT entry points taking an ignored `QueryType`, the public `query*` wrappers (three-argument and the two-argument overloads), and the 26 option members removed from the interface, the Impl and the six test classes (T104). `test_shading_contract_typemismatch_attr` is no longer `EXCLUDE_FROM_ALL` (T105).

Three findings stopped the first checkpoint run; the maintainer approved all three on 2026-11-01 (D9 as T105a; T1 and T2, the edits of `case_type_mismatch_real.cpp` and `case_null_object.cpp` in `tools/phase5-test-fixes.diff`, approved test edits) and they are applied:

| Finding | Effect before the fix | Fix |
|---|---|---|
| D9, the JIT reads a color, normal or point destination as a float (`llvmEmitter.cpp`); `rendererinfo(name, vector)` resolves to the `f=SC` overload (declared before `f=SV`) | `AttributesJit`, `RealAnswersJit_Attributes`, `TypeMismatch_AttributeJit`, `TypeMismatchReal_AttributeJit` failed: the JIT asked `Float` where the interpreter asks `Triple` | `tools/phase5-d9-emitter.diff` (C, N and P map to 3, like the interpreter's `DEFLINKFUNC`) |
| T1, the two new rendered tests installed their diagnostic sink before `RiBegin`, which replaces it | `TypeMismatchReal_AttributeInterp` failed its warning counts; `NullObject_Attributes` counted 0 trivially | `tools/phase5-test-fixes.diff`: install the sink after the session starts, as `case_real_answers.cpp` does |
| T2, GCC 13 `-Werror=array-bounds` in `userAttributes.h:186` (the string branch of `insert()` seen against a one-float object; the family of D2) | Linux build of `test_shading_contract_typemismatch_attr` failed | same diff: the insert goes through a `noinline` helper that takes `const void *` |

Results on the final staged tree (`linux_sync.sh push` then `verify`: 3,226 files, **0 differences**):

| Check | macOS | Linux (`openrender-026`, GCC 13.3 `-Werror`, `-j2`) |
|---|---|---|
| build | clean | clean (exit 0) |
| the 12 Phase 5 tests | **12 of 12 pass** | `shading_contract` label 29 of 29 |
| CP-b `-LE determinism-sweep` | 959 tests, 100% passed | 960 tests, 953 passed, **7 failed = the Linux baseline's 7** |
| CP-d (fresh stamp; the `oshader` rebuild recompiled every shader) | 819 images, **0 differing, 0 missing**, `MaxBlockAvgDiff` equal; 9 INFO lines (cwd `ri.tif` files of the contract tests) | 0 differing, 0 missing, equal diffs; 9 INFO |
| CP-c | 21 of 21, `RESULT PASS`, `ALLOWED` unchanged | 21 of 21, `RESULT PASS`, `ALLOWED` unchanged |
| `libshader_shading` imports (SC-011) | the same 6 flat-namespace imports | `diff` against `libshader-imports-base-linux.txt` empty |

## §3 Linux records

Note for the reader: the Linux baseline is taken on the starting `src/` **plus the Phase 1 observer hook** (the macOS baseline is taken before it). The hook is null in production and adds no output, which the macOS manifest check shows (§1); the Linux baseline is therefore comparable with the starting tree.

**Tree**: `juvenal@ubuntu-2404-desktop.v2-labs.io:~/Projects/Development/CLI/openrender-026` (created; `openrender` and `openrender-037` untouched), GCC 13.3.0, cmake 3.28.3, Release, JIT on (`.slo` built), 2 cores. Remote jobs through `~/.claude/bin/cpujob` at `-j2`; the first build ran with `CPUJOB_TIMEOUT=7200` (the build itself needed far less). The lock was shared with spec 037 (a few jobs queued behind its suite).

| Step | Result |
|---|---|
| T035 `linux_sync.sh push` / `verify` | pushed 3,139 files; **3139 files, 0 differences**; `no-git-ok` (no `.git` in the remote tree). The recipe worked with the local openrsync against the remote rsync 3.2.7 |
| T035 configure (`cpujob 026-l1-configure`) | clean |
| T036 build, first attempt | **failed under `-Werror`: 4 `-Warray-bounds` errors (GCC 13 false positive) in my test header `replay::backends()`** (`std::vector<std::string> b = {"rslo"}; b.push_back("slo");`, inlined into `trace_replay_check.cpp`). Not in `src/`, not in the hook. Fixed (a single initializer list, no `push_back`), re-pushed (3,139 files, 0 differences) and rebuilt: **exit 0, no warning**. See "Deviations" in the T043 report: this is an edit to test code the maintainer approved at T016 and is listed for ratification |
| T037 `ctest -L architecture` | 21 of 21 passed. `tools/archcheck-base-linux.txt`: `ALLOWED DB-001 x 8 ... DB-005 x 8 ... DB-006 x 33`, `METRIC Shading 0`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; the five deletion keys are present (no platform tag) |
| T038 record, run twice (first attempt, before the D1 fix) | **NOT byte-identical** (`cmp`: differ at byte 20240, line 110). See defect D1. 14 of the 16 sections are identical between the two runs and match macOS in request counts; the `.slo` sections of `transmission` and `shadow` differ run to run. `tests/shading_contract/replay/linux.txt` was therefore **not** kept (deleted from the remote tree); both runs are kept as evidence in `tools/replay-nondeterminism-linux-run{1,2}.txt` |
| T039 suite 1 (`-LE determinism-sweep -E '^ShadingContract_TraceReplay_' -j2`) | **931 tests, 924 passed, 7 failed**: `CoverageAnalytic_alpha-sides2-tjunction-r4_reyes`, `TextureTile_RenderByteIdentical`, `TextureTile_CacheByteIdentical`, `TextureTile_CacheDefaultDirectory`, `TextureTile_CacheStaleDetection`, `TextureTile_CacheConcurrentWrite`, `LibShader_McompConsistency`. These are the Linux baseline's failing set (the 7 of the earlier specs: #44, #45, #46, #57). The 8 `TraceReplay_*` checks are excluded because no `linux.txt` exists; the recorder test ran. Manifest: 819 images; `baseline-diffs-linux.txt` 389 entries; log `tools/base-LastTest-linux.log` |
| T039 suite 2 (noise) | same 7 failures; 819 images, **0 differing**; every `MaxBlockAvgDiff` equal; `baseline-image-noise-linux.txt` is empty (comment only) |
| T040 sweep 1 (`-L '^determinism-sweep$' -j2`) | 419 tests, 417 passed, **2 failed**: `DeterminismSweep_sphere-texture3d-bake3d-reyes` and `..._reyes-slo`. 419 images in `baseline-sweep-images-linux.sha256` |
| T040 sweep 2 | **419 of 419 passed**; 2 images differ from run 1: the same two bake3d scenes. They are the Linux sweep noise (`baseline-sweep-noise-linux.txt`) and flaky on the starting tree (a failure that predates this spec) |
| T041 `nm -D --undefined-only` | the same 6 imports as macOS (`CPhotonMap::lookup`, `CGatherBundle::CGatherBundle()`, `CShadingContext::trace(CRay*)`, `CShadingContext::traceEx(CRayBundle*)`, `vtable for CTraceBundle`, `vtable for CTransmissionBundle`) among libstdc++/glibc ones. `readelf -d`: **no `FLAGS` entry, so no `BIND_NOW`**: function imports bind lazily through the PLT; the vtables are data relocations and resolve at load. So the Phase 2 load stub must define the 2 vtables; the undefined functions are only needed if something calls them (they are not bound at load) |
| T042 `linux_sync.sh pull` | 10 files, 0 differences (all `*-linux*` files below). `replay/linux.txt` does **not** exist (see D1) |

| T042e after the D1 fix | record twice **byte-identical**, counts equal macOS (§4.4 D1); `linux.txt` recorded |
| T042f after the D1 fix | contract 8 of 8; suite 939 tests, the 7 baseline failures only, images 0 differing; sweep 419 of 419, 0 differing; architecture 21 of 21; `pull` 11 files and `verify` 3,140 files, 0 differences |

Files pulled: `archcheck-base-linux.txt`, `baseline-images-linux.sha256`, `baseline-diffs-linux.txt`, `baseline-image-noise-linux.txt`, `libshader-imports-base-linux.txt`, `libshader-dynamic-base-linux.txt`, `base-LastTest-linux.log`, `baseline-sweep-images-linux.sha256`, `baseline-sweep-noise-linux.txt`, `base-sweep-LastTest-linux.log`.

### Phase 3: Linux (T080; `openrender-026`, GCC 13.3, `-Werror`, `-j2`)

The host did not resolve at the first attempt (three tries, 2026-10-31) and was reachable again after the maintainer's check; the run below is on the final tree (the two confirmed test fixes applied, the four Phase 3 tests ENABLED).

| Check | Result |
|---|---|
| push / verify | 3,213 files pushed; **3213 files, 0 differences** |
| configure | clean; `-UOPENRENDER_SHADING_CONTRACT_TESTS` (the cache held the Phase 2 value), now `ON` |
| build | exit 0, no warning (`-Werror`), both contract programs built against the real headers |
| CP-b `-LE determinism-sweep -j2` | **943 tests, 936 passed, 7 failed: exactly the Linux baseline's 7** (`CoverageAnalytic_alpha-sides2-tjunction-r4_reyes`, `TextureTile_RenderByteIdentical`, `TextureTile_CacheByteIdentical`, `TextureTile_CacheDefaultDirectory`, `TextureTile_CacheStaleDetection`, `TextureTile_CacheConcurrentWrite`, `LibShader_McompConsistency`). No new failure |
| CP-d (fresh stamp) | 819 images, **0 differing, 0 missing**, against `baseline-images-linux.sha256`; every `MaxBlockAvgDiff` equal to `baseline-diffs-linux.txt`; 1 INFO (the stray `ri.tif`, as on macOS) |
| CP-m | `ctest -L '^shading_contract$'`: the 8 replay tests, `StandaloneLinkLine` and the 4 new tests pass (12 passed lines), the other 24 "Not Run (Disabled)", exit 0 |
| CP-c | `ctest -L architecture` 21 of 21; `ALLOWED` DB-001 x 8, DB-004 x 52, DB-005 x 8, DB-006 x 33, DB-010 x 8, DB-011 x 166, DB-015 x 10, DB-017 x 78, DB-019 x 3, DB-021 x 3, DB-025 x 12; `RESULT PASS` |
| standalone program, `nm -C --defined-only` | defines all 6 stub symbols: `CPhotonMap::lookup`, `CGatherBundle::CGatherBundle()`, `CShadingContext::trace(CRay*)`, `traceEx(CRayBundle*)`, `vtable for CTraceBundle`, `vtable for CTransmissionBundle`; none of them is an undefined import of the program. `readelf -d`: NEEDED `libshader_shading.so.1`, `libRIBCommon.so.1`, libstdc++, libgcc_s, libc; **`BIND_NOW` is set** (flags NOW, PIE), so an undefined symbol would fail at start: the stub's table equals the measurement and needs no Linux adjustment |
| `libshader_shading.so` imports (`nm -D --undefined-only`, demangled, sorted) | **identical** to `libshader-imports-base-linux.txt` (465 lines; `diff` empty): SC-011 holds on Linux. Saved as `tools/libshader-imports-phase3-linux.txt` |

The suite counts differ by one between the platforms (943 on Linux, 942 on macOS), as they did at Phase 1 (939 against 938): a platform-gated test, unchanged by this phase.

### Phase 4: Linux (T090; `openrender-026`, GCC 13.3, `-Werror`, `-j2`), after T086a

| Check | Result |
|---|---|
| push / verify | 3,213 files, 0 differences |
| build | exit 0, no warning |
| CP-b `-LE determinism-sweep -j2` | 948 tests, 941 passed, **7 failed: exactly the Linux baseline's 7** (before T086a the two JIT Matrices tests failed too, D8) |
| CP-d (fresh stamp) | 819 images, 0 differing, 0 missing; `MaxBlockAvgDiff` equal to `baseline-diffs-linux.txt`; 3 INFO (the cwd `ri.tif` files) |
| CP-m | `ctest -L '^shading_contract$'`: 17 of 17 pass |
| CP-c | 21 of 21; `ALLOWED` identical; `RESULT PASS` |

## §4 Findings: triage and filed issues

**Rule (maintainer, 2026-10-31)**: a finding whose context matches or impacts this spec is resolved in spec 026.
Anything else is filed on GitHub as P1 (the existing label `priority: high`). O1 to O7 are filed as #115 to #121; the implementer
files nothing and adds evidence as a comment on the issue. The text in §4.3 is the body as filed. The decisions are recorded in spec.md, Clarifications, "Session 2026-10-31".

### §4.1 Classification

| # | Finding | Class | Reason | Where it is resolved |
|---|---|---|---|---|
| F1 | `attribute()`/`option()`/`rendererinfo()` and message passing never check the type (R16.1): a stored value of another type class or a larger fixed size is written into the destination | **IN-SPEC** | The copy is in the Shading members this spec rewrites, and the `TypeMismatch` status is this spec's own contract; leaving a reported-but-ignored status over a known overflow defeats it. Narrowed to what cannot change a correct output (see FR-026). Supersedes Q4's "treated as Found". | FR-026, SC-013; tasks in Phases 5 and 6 (tests first) |
| F2 | A query with no current object dereferences a null pointer: `attribute()`, `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()`, `shadername(type)` (R16.5; the imager sets `currentObject = nullptr`) | **IN-SPEC** | Q12 already makes the services answer `NotFound`; the same builtins' other reads are in the members this spec rewrites. Extends Q12 to `shadername(type)` (Q8 keeps it a direct read, now null-guarded). Only a path that crashes on master changes. | FR-027, SC-014; Phases 5 and 6 |
| F3 | The interpreter's `lightsource()` outside an `illuminance`/`illuminate` loop reads through a null `currentLight` (R7) | **IN-SPEC** | The macro is rewritten to go through `lightParameter()`/`getMessage` here (Q2). Only a path that crashes on master changes. The JIT already guards it. | FR-028, SC-014; Phase 6 |
| F4 | `incident()`/`opposite()` pass `ACCESSOR_EXTERIOR`/`ACCESSOR_INTERIOR` the other way round from the source Shading asks for, in the interpreter (`shaderFunctions.h:1331-1344`) and in the JIT (`jitIncidentParameter`/`jitOppositeParameter`, `shading.cpp:3846, 3854`; R7) | **IN-SPEC** | The `DEFFUNC` lines are edited in Phase 5 anyway, and the two JIT members sit beside the code that gains the expected type. The accessor is never read for these two (no variable comes back), so no output changes. | FR-029; Phase 5 |
| O1 (#115) | JIT `lightsource()` differs from the interpreter: reads the last light that ran, floats only, ignores mutable values, writes the value where the found flag belongs (R16.2) | **OUT-OF-SPEC** | Q2 (maintainer, 2026-10-30) decided each backend keeps its output, because making them equal changes the JIT's images (FR-022); spec 033 owns the JIT's message-passing entry points (DB-018). Reaffirmed 2026-10-31. | #115 |
| O2 (#116) | `CDLObject::shade` calls `shade(this, numRays, -1, SHADING_2D, 0)` (R16.3, situation oddity 4) | **OUT-OF-SPEC** | A Geometry source (`dlobject.cpp`) that 026 does not edit; the fix changes DSO-object behaviour (today an assert in Debug and a negative vertex count in Release); spec 031 owns Geometry. Memory-safety. | #116 |
| O3 (#117) | `camerahitmode` is honoured only for REYES grids, after the shaders, and not by raytrace camera hits or 0-D points (R16.4; situation oddities 1, 2, 8) | **OUT-OF-SPEC** | Fixing it changes images and makes shading depend on the hider differently; the spec records "today's inference, oddities included" (Assumptions) and FR-022 forbids an output change. Spec 029 reads the ray type. | #117 |
| O4 (#118) | `diffusehitmode` is parsed and warned about, never read; gather hits use `specularhitmode` (R16.4; oddity 5) | **OUT-OF-SPEC** | Same reason as O3; the parser is Scene Interface (`rendererContext.cpp`), not Shading. | #118 |
| O5 (#119) | Implicit-surface ray hits are shaded as `SHADING_0D`: no derivatives, no hit mode (oddity 3) | **OUT-OF-SPEC** | Geometry source (`implicitSurface.cpp`); the fix changes images of implicit surfaces under filtered lookups; spec 031. | #119 |
| O6 (#120) | Three behaviours to verify against the RISpec: atmosphere only at ray depth 0 and never on a matte object (oddity 6); a shaded transmission hit skips the displacement shader (oddity 7); `incident()` always reads the interior shader and `opposite()` the exterior one, whatever side the ray is on (R7) | **OUT-OF-SPEC** | They look intentional, are documented in the situation table, and fixing any of them changes images. Filed as one verification issue, which the maintainer may close as by-design. | #120 |
| O7 (#121) | The destination size of a float-class query is unknown: a scalar `float` receiving a multi-float stored value overflows (residual of R16.1 after F1) | **OUT-OF-SPEC** | F1 refuses what can be told without the destination's length. RSL passes no length to the builtin, so the rest needs a compiler/ABI change (spec 033). Memory-safety. | #121 |

Not findings (checked and dropped): `options()` without services used to return 0 silently and now warns once, which
FR-005/R9 already specify; `name[0]` being used for a varying name argument is master's documented
resolve-once-then-broadcast discipline; the known GitHub issues #8, #9, #10, #11 are already filed. No existing open
issue matches O1 to O7 (searched `lightsource`, `vVertices`, `camerahitmode`, `diffusehitmode`, `null`, `overflow`,
`implicit`).

### §4.2 What the implementer records for the in-spec findings

F1 to F4 are fixed by tasks, tests first (tasks.md, Phases 5 and 6). F1 does not apply to source `Light` (B4: `lightsource()` keeps each backend's output). They are **not** "record only": each has a
requirement (FR-026 to FR-029), an acceptance line and a test that is red before the fix. The implementer records in
§2 the red output of each test and in §1 the CP-d result that shows no image changed. If a CP-d difference appears
for F1, F2 or F3, a scene of the suite reaches a path that was undefined behaviour: stop and report; do not widen
anything.

### §4.3 Issues (OUT-OF-SPEC; filed 2026-10-31 as P1: #115–#121)

Labels come from the repository's existing set: `bug`, `jit`, `memory-safety`, `needs-recheck`, `priority: high`
(the P1 label). Reproductions are by code reading unless stated; none was run (the tasks stage builds nothing). Item 3 of #120 (the crossed accessors) goes stale when F4 lands; the coordinator updates that issue.

#### O1 (#115). JIT `lightsource()` does not match the interpreter

Labels: `bug`, `jit`, `priority: high`

The same `lightsource()` call gives different results under `.rslo` and `.slo`.

| | Interpreter (`LIGHTPARAMETEREXPR_PRE`) | JIT (`op_lightsource_f`) |
|---|---|---|
| Light read | the light being iterated, `currentLight->instance` | `currentShadingState->currentLightInstance`, the last light shader that ran |
| Mutable (varying) parameter | read from the light's saved state | ignored (`getParameter(..., nullptr, nullptr)`) |
| Types | float, vector, string, matrix | float only |
| Return value | the found flag | the parameter's value, written into `result` and `outParam` |

- `src/libshader/shading/shaderFunctions.h:1199-1230` (interpreter)
- `src/libshader/shading/rslOps.cpp:918-937` (`op_lightsource_f`)
- `src/libshader/compiler/llvmEmitter.cpp:3113-3131` (the emitter call)

Reproduction: two lights with different `float k` values; a surface that does `illuminance(P, N, PI/2) { lightsource("k", kk); Ci += kk; }`. Render with `.rslo` and with `.slo` and compare. With one light the two agree; with two they do not.

Context: found while writing spec 026. Spec 026 routes both backends through `getMessage` and keeps each output (clarification Q2), so this is not fixed there. Spec 033 owns the JIT's message-passing entry points (registry DB-018).

#### O2 (#116). `CDLObject::shade` passes `vVertices = -1`

Labels: `bug`, `memory-safety`, `priority: high`

`context->shade(this, numRays, -1, SHADING_2D, 0)` makes `numVertices = uVertices * vVertices = -numRays`. `shade()` asserts `vVertices > 0` and `numVertices > 0`. In Debug that aborts; in Release a negative count is used as a vertex count. The two other ray-hit callers pass 1.

- `src/ri/geometry/dlobject.cpp:176` (the call)
- `src/libshader/shading/shading.cpp:855-876` (the asserts and the product)
- `src/ri/render/geometryDispatch.cpp:405` and `src/ri/geometry/implicitSurface.cpp:258` (the callers that pass 1)

Reproduction (not run): raytrace hider, any DSO geometry object (`CDLObject`) hit by a ray, in a Debug build: the assertion at `shading.cpp:866` fires.

Likely fix: pass 1. Found while writing the situation table of spec 026 (row 7); spec 026 edits no Geometry source.

#### O3 (#117). `camerahitmode` is honoured only for REYES grids

Labels: `bug`, `priority: high`

`Attribute "shade" "camerahitmode" "p"` ("look at the primitive, do not run the shaders") has an effect only in one place, and a different one from the others:

- REYES grids: `shading.cpp:1374-1382` applies it after the shaders have run, and overwrites `Oi` with the attribute opacity while keeping `Ci`.
- Raytrace camera hits go through `trace()` -> `CSurface::shade` as `SHADING_2D` and read `specularhitmode` (`shading.cpp:915`) instead.
- REYES points and curves are shaded as `SHADING_0D` (`src/ri/hiders/reyes.cpp:907, 968`), and the test at `shading.cpp:915` needs `SHADING_2D`, so they get no hit mode at all.

So one attribute gives three results depending on the hider and the primitive, against Principle VIII rule 3 ("shading results must not depend on which hider drives evaluation").

Reproduction (not run): the same object with `camerahitmode "p"` rendered with `Hider "hidden"` and `Hider "raytrace"`: the raytrace image still runs the surface shader.

Context: situation-table oddities 1, 2 and 8 of spec 026 (`specs/026-shading-contract-v2/contracts/shader-globals.md`). The fix changes images, so it is outside spec 026; spec 029 reads the ray type.

#### O4 (#118). `diffusehitmode` is parsed and never read

Labels: `bug`, `priority: high`

`Attribute "shade" "diffusehitmode"` is stored (`src/ri/state/attributes.cpp:131`, queryable at `:682`) and any value other than `"p"` warns `shading of diffuse rays not currently supported` (`src/ri/render/rendererContext.cpp:3701-3704`). Nothing reads `diffuseHitMode`, so gather and indirect hits are decided by `specularhitmode` (`src/libshader/shading/shading.cpp:915`).

Reproduction: `Attribute "shade" "diffusehitmode" "s"` prints the warning; set `specularhitmode "p"` and `diffusehitmode "s"` and a `gather()` hit is still not shaded.

Context: situation-table oddity 5 of spec 026.

#### O5 (#119). Implicit-surface ray hits are shaded 0-D: no derivatives, no hit mode

Labels: `bug`, `priority: high`

`src/ri/geometry/implicitSurface.cpp:258` calls `context->shade(this, numRays, 1, SHADING_0D, 0)`. `shade()` skips the derivative computation for 0-D points (`shading.cpp:1063`: "we can not differentiate a 0 dimentional point set") and the hit-mode test at `shading.cpp:915` needs `SHADING_2D`. A ray-hit implicit surface therefore has no `du`/`dv` and ignores `specularhitmode`/`transmissionhitmode`.

Reproduction (not run): a blobby (`RiBlobby`) object with a filtered `texture()` lookup under the raytrace hider versus the same texture on a quadric.

Context: situation-table oddity 3 of spec 026; spec 031 owns Geometry.

#### O6 (#120). Verify three shading behaviours against the RISpec

Labels: `needs-recheck`, `priority: high`

Each looks intentional; none has been checked against the RenderMan Interface Specification, and changing any changes images:

1. The atmosphere shader runs only at ray depth 0 (`shading.cpp:1361`, "do not execute atmosphere for non-camera rays") and never on a matte object (`:933-936`).
2. A shaded transmission/shadow hit skips the displacement shader (`shading.cpp:905-909`, `displacement = NULL`).
3. `incident()` always reads the interior shader and `opposite()` the exterior one (`shading.cpp:1636-1663`), whatever side of the surface the ray is on; the interpreter macros even pass the accessors the other way round (`shaderFunctions.h:1331-1344`, harmless: no variable comes back).

Context: situation-table oddities 6 and 7 and research R7 of spec 026. Close as by-design if the maintainer agrees.

#### O7 (#121). A float-class destination of unknown size is overflowed by a multi-float value

Labels: `bug`, `memory-safety`, `jit`, `priority: high`

`attribute()`, `option()`, `rendererinfo()`, `surface()`, `displacement()`, `atmosphere()`, `incident()` and `opposite()` copy the stored value's full size into the shader's destination, and the builtin is never told how large the destination is (RSL passes no array length). Spec 026 refuses what can be told apart (a string against a number, a matrix against a vector), but a stored `color` or `float[3]` asked for through the scalar-`float` overload still writes 3 floats into one. Both backends are affected (the JIT passes `resultKind` only).

- `src/libshader/shading/shaderFunctions.h:1161-1230` (the copy macros)
- `src/libshader/shading/shading.cpp` `attributes()`, `options()`, `rendererInfo()` (about `:1665-1903`)
- `src/libshader/shading/shader.cpp:459` (`CProgrammableShaderInstance::getParameter`)

Reproduction (not run): `Attribute "user" ...` holding a color, then in a surface `uniform float f; attribute("user:c", f);` followed by neighbouring locals; run under AddressSanitizer or print the neighbours.

Needs the destination length from the compiler (a signature or ABI change for these builtins); belongs with spec 033. Found while specifying spec 026's `TypeMismatch` rule.

### §4.4 Defects found while implementing (not filed; for the maintainer)

#### D1. The replay reads uninitialized memory: `N` for `transmission()`/`visibility()` (both backends), and `L` for the `shadow` scene (found at T038, cause found at T042a)

Found by the trace replay on Linux (GCC 13.3, Release): two recordings differ in the `.slo` sections of `transmission` and `shadow` (see the first investigation below). **Cause, part 1 (proved): `N` is never filled for `transmission()`, `visibility()` and the float/colour `trace()`.**

- `shade()` copies `Ng` into `N` only `if (usedParameters & PARAMETER_N)` (`shading.cpp:218, :367, :994`). The shader's `usedParameters` is the union of the `PARAMETER_*` flags its builtins declare. `Visibility`, `Transmission`, `TraceF` and `TraceV` (`giFunctions.h:126, 127, 171, 172`) declare `PARAMETER_RAYTRACE | PARAMETER_DERIVATIVE | PARAMETER_DU | PARAMETER_DV (| PARAMETER_TIME)` but **not `PARAMETER_N`**, although all four read `N`: the interpreter's `TRANSMISSIONEXPR` does `movvv(rays->N, N)` and the JIT's `jitTraceBatch` reads `Nrm` (`JIT_IDX(Nrm, 3, i)`), and `traceTransmission`/`traceReflection` pass `rays->N` to `keepOriginAbove(from, N, dir)` (`executeMisc.cpp`, `sampleRay`). (`gather` declares `PARAMETER_N`; that is why it is stable.) A shader that calls one of them and does not read `N` itself therefore leaves `N` unwritten, and the ray origin's tangent jitter is projected against whatever the arena held.
- The maintainer's suspect (a collapsed `N`/`time`/`du`/`dv` stride in `jitTraceBatch`) is **not** the cause: the emitter passes the real global variable arrays (`resolveVar("du" / "dv" / "N" / "time")`, `llvmEmitter.cpp` ~2815), which are never uniform-collapsed, and `P`/`D` carry their real strides (`sP = sD = 3` in the log).
- Reproduction on macOS, where the plain runs are stable: `MallocPreScribble=1 MallocScribble=1 ctest -L '^shading_contract$'` (fresh arena pages filled with `0xaa`) fails exactly `ShadingContract_TraceReplay_transmission` and `ShadingContract_TraceReplay_visibility`, **in both `.rslo` and `.slo`**: `request 0 ray 1 field from differs` / `request 1 ray 0 field from differs`. Temporary logging in `traceTransmission` (removed) showed `N=-0x1.555554p-42,-0x1.555554p-42,-0x1.555554p-42` (= `0xAAAAAAAA`) for every ray, with `P`, `D`, `dPdu`, `dDdu`, `sampleBase`, `bias` and `time` all sane. On Linux the stale arena content is non-zero and differs between runs and between the backends (the JIT and the interpreter reach the arena with different histories), which is why only the `.slo` sections differed there.
- Why the other families are stable: the `trace`, `environment`, `occlusion` and `indirectdiffuse` replay shaders read `N` (or pass it) themselves; `gather` declares it.
- **Cause, part 2 (not proved; Linux unreachable since): the `shadow` scene calls `shadow("raytrace", P)` from a surface shader.** `SHADOWEXPR_PRE` and `jitEnvironment` form `rays->D = Ps - L` and the derivatives of `Ps - L` from the global `L`, which only a light shader defines; in a surface shader `L` is also never filled (no `PARAMETER_L`). The Linux `.slo` shadow section showed `da=-nan` (8 values) and a different ray count (13 against 20), which fits a garbage `L` feeding `computeRayDifferentials`. The macOS scribble run does not show it because `0xAAAAAAAA` is about -3e-13, below the resolution of `Ps`. This is a defect of the replay scene (shadow() outside a light shader), not of the product.

**Trial fix (reverted, nothing staged)**: add `| PARAMETER_N` to the four `DEFSHORTFUNC` lines of `giFunctions.h`; rebuild (the shaders recompile). macOS result: 938 tests, **only** `ShadingContract_TraceReplay_transmission` and `_visibility` fail (against `macos.txt`; first lines `request 0 ray 0 field from differs: recorded from=-0x1.4192a8p-2,0x1.f3ec44p-2,0x1.0918dap+2, fresh from=-0x1.3eab56p-2,0x1.f104f4p-2,0x1.09b5c6p+2` and `request 1 ray 0 field from differs`, both backends); the visual suite is untouched: CP-d **819 images, 0 differing, 0 missing**, every `MaxBlockAvgDiff` equal. So the fix changes no image of the suite, **but it changes bytes of the approved `macos.txt`**: its `transmission` and `visibility` sections recorded rays whose origins were computed from uninitialized `N` (in plain macOS runs, a zero or stale value). This is the stop condition of the decision of 2026-10-31; it is reported, not worked around. The `shadow` section would need its scene rewritten (light shader), which changes that section too.

**Fix applied (option A, maintainer 2026-10-31) and results.**

1. `giFunctions.h`: `| PARAMETER_N` added to `Visibility`, `Transmission`, `TraceF`, `TraceV` (4 lines; the only `src/` change of the fix).
2. The `shadow` replay scene now calls `shadow("raytrace", Ps)` from a light shader: `tests/shading_contract/replay/shaders/replay_shadow.sl` is `light replay_shadow(point from = point (0, 4, -2))` with `illuminate(from) { ... }` holding the two call shapes, and `scenes/replay_shadow.rib` has `LightSource "replay_shadow" 1` and `Surface "matte"` on the sphere. A first attempt with the default `point "world" (0, 4, -2)` made the `.rslo` and `.slo` sections differ (rslo 10 rays, slo 20; different hit points and `t`): a light parameter default in a named space is evaluated differently by the interpreter and the JIT (finding D4 below); the plain `point (0, 4, -2)` removes it.
3. **macOS** re-recorded twice: `replay-fresh.txt` byte-identical (also against two further runs). Against the approved `macos.txt`: **only the `transmission`, `visibility` and `shadow` sections changed** (both backends: 80 ray lines, same request and ray counts: 2/20, 8/8, 2/20); the `trace`, `gather`, `environment`, `occlusion` and `indirectdiffuse` sections (10 of 16) are **byte-identical** to the approved file (checked section by section). Every `req` line is unchanged. The diff is `tools/macos-replay-rerecord-d1.diff`. `.rslo` and `.slo` sections are identical for all 8 families.
4. **Determinism control** `MallocScribble=1 MallocPreScribble=1 ctest -L '^shading_contract$'`: 8 of 8 pass (before the fix: transmission and visibility failed under it).
5. **CP-b/CP-d/CP-m/CP-c on macOS**: full suite 938 of 938 passed; images 819, **0 differing, 0 missing**, every `MaxBlockAvgDiff` equal to the baseline; the determinism sweep 419 of 419 passed, **0 differing, 0 missing**; `ctest -L architecture` 21 of 21; `ctest -L '^shading_contract$'` 8 of 8. The fix changes no image of the suite or of the sweep.
6. **Linux** (push 3,140 files, 0 differences; GCC 13 `-Werror` build at `-j2`, no warning): the record run twice gives **byte-identical** `replay-fresh.txt`; request/ray counts equal macOS for all 16 sections (trace 10/28, transmission 2/20, visibility 8/8, gather 12/48, environment 3/24, shadow 2/20, occlusion 48/48, indirectdiffuse 48/48); `.rslo` and `.slo` sections are identical for every family (before: two differed). Copied to `tests/shading_contract/replay/linux.txt`. `ctest -L '^shading_contract$'` 8 of 8; the Linux suite (`-LE determinism-sweep -j2`, `TraceReplay_*` included) 939 tests, the same 7 failures as the baseline set and no other, images 819 with **0 differing**, every `MaxBlockAvgDiff` equal; the Linux sweep 419 of 419 passed with **0 differing** against the Linux sweep manifest (2 noise-listed); `ctest -L architecture` 21 of 21; `pull` 11 files, 0 differences, then `verify` 3,140 files, 0 differences.

#### D4 (#123). (found while fixing D1) A light parameter default in a named space differs between the interpreter and the JIT

`light replay_shadow(point from = point "world" (0, 4, -2))` under `matte`: the `.rslo` run shaded 2 points and made rays of `t` about 3.9, the `.slo` run shaded 4 points and `t` about 7.4. With the default written in the current space the two agree. Not investigated further (outside 026); recorded for the maintainer. Possibly related to the "shader-space parameter defaults at bind time" fix of the JIT (`jitSetInitXform`).

**First investigation (T038)**, kept for the record: on Linux 14 of the 16 sections are identical between two runs; `transmission` `.slo` and `shadow` `.slo` differ (from, dir and t; one shadow request carries `da=-nan`); the `.rslo` sections of the same scenes are stable on Linux. Evidence: `tools/replay-nondeterminism-linux-run{1,2}.txt`.

#### D2. GCC 13 `-Warray-bounds` false positive on `std::vector<std::string>{...}` followed by `push_back`

Not a defect of the product: a known GCC 13 diagnostic, hit by the new replay test header (see §3, T036). The fix is test-side.

#### D3. Linux sweep: `sphere-texture3d-bake3d-reyes` and `-slo` are flaky on the starting tree

Run 1 failed both, run 2 passed both, and their images differ between runs. macOS: no such flakiness. Recorded as the Linux sweep noise list; not this spec's.


#### D5. `CContractServices::retrieveVariable` answers null, so the shader loader rejects every shader (found at T075/T077; approved test code; fix confirmed by the maintainer 2026-10-31 and applied)

`parseShader` asks the services for every global a shader names (`Ci`, `P`, ...); the double returned `nullptr`, so `cm_producer_surface.rslo` failed with `Unknown variable` and `StandaloneLink` could not start. Proposed fix in `tools/phase3-test-fixes.diff` (`contract_services.h`): look the name up in the double's own table of the 29 globals. Test code only; a semantic failure of an approved test, so it waited for the maintainer's confirmation, which came 2026-10-31.

#### D6. Suspected Apple clang 21.0.0 `-O2` miscompile of the key copy in `case_globals_double.cpp` `make()` (found at T077; approved test code; fix confirmed by the maintainer 2026-10-31 and applied)

`b.keys[numPoints + t] = b.keys[t % numPoints]` (a tail copy within the same vector) comes out zero in Release (`ShaderGlobalsDouble`: 32 of 399 checks fail). The cause is suspected, not confirmed: the original loop is legal C++; `-O1`, `-O0`, ASan and UBSan all pass. A standalone reproduction was made in the scratch directory. Proposed fix (same diff, `case_globals_double.cpp`): build each block from `keyOf(p)` again. Not a defect in `src/`. On Linux (GCC 13) the original loop was not tried: the confirmed fix is in the tree, and the Linux Phase 3 table (§3) is green with it.

#### D7. The 25 coordinate-system call sites do not all reach `CShadingContext::findCoordinateSystem`: at bind time `init.cpp` redirects the macro (found by the #123 diagnosis, recorded 2026-10-31)

The T008 count (25 textual sites) and the claim "all reach one member" (research R-coordinate-systems, plan.md, T086) are right for shading. `src/ri/render/init.cpp:60` has `#define findCoordinateSystem CRenderer::findCoordinateSystem` and its interpreter switch includes `scriptOpcodes.h`/`scriptFunctions.h` (-> `shaderOpcodes.h`, `shaderFunctions.h`), so the `pfrom` site (confirmed) and, by the same chain, the other macro sites expand there into a direct core call. Not a defect and no behaviour change: `T086` keeps those, the text of T086, plan.md and research.md is corrected, and T086 counted the `init.cpp` expansions with `-E -P` and the build's own flags (T086, 2026-10-31): **19**, every macro site (17 in `shaderFunctions.h`, 1 in `giFunctions.h`, 1 in `shaderOpcodes.h`), each as a direct `CRenderer::findCoordinateSystem(` call; the other `findCoordinateSystem(` tokens in the preprocessed text are three declarations (`renderer.h`, `shading.h` x2). So all 19 macro sites are instantiated twice, once for shading (reaching the Shading member) and once for bind time (reaching the core). The #123 cause is the JIT's bind-time fallback in `rslOps.cpp` plus `init.cpp`, fixed on `fix/123-named-space-light-default`; Phase 4 gets no #123 task.

#### D8. The JIT does not lower the two-space `ctransform("from", "to", c)` form (found at T088; pre-existing; `Matrices` and `RealAnswers` JIT fail on it)

`llvmEmitter.cpp` handles `cfrom`, `mfrom` and `ctransform` in one branch that reads `operands[0]` as the space and `operands[1]` as the colour source. That is the `c=Sc` form. The `c=SSc` form (`addBuiltInFunction("ctransform", "c=SSc", 0)`, `CTRANSFORMSEXPR` in `shaderFunctions.h`) has operands `{from, to, src}`, so the branch looks up one space name (the first) instead of two and reads the second space name as the source. The `transform`/`vtransform`/`ntransform` branch above it already dispatches on the proto shape (`S`, `m`, `SS`, `Sm`); `ctransform` has no such dispatch. Visible effects under `.slo` on the unchanged emitter: `cm_matrices` case 8 asks the double for 1 name where the interpreter asks for 2 (`rgb`, `hsv`), and the converted colour is wrong at every lane (12 value checks, 4 points x 3). Not introduced by this spec: nothing in the emitter or `rslOps.cpp` changed. The approved tests (`Matrices{Jit}`, `RealAnswersJit_Matrices`) expect the interpreter's behaviour, so Phase 4 cannot go green without one of: (A) lowering the `SSc` form in the emitter with a new `op_ctransform_ss` in `rslOps.cpp` (mirrors `op_*transform_ss`; the two lookups then go through `jitFindCoordinateSystem` like every other site); or (B) an edit to the approved test cases (needs approval). **Decided (maintainer, 2026-10-31): option A, no issue filed.** Fixed in 026 as T086a: the emitter's `ctransform` branch now dispatches on the proto shape (`SSc`) and calls the new `op_ctransform_ss` (`rslOps.cpp`, next to `op_ctransform`; `rslOps.h`), which resolves both names through `getColorCoordinateSystem` (-> `jitFindCoordinateSystem`) and converts `from -> rgb -> to` as `CTRANSFORMSEXPR` does. FR-009 (spec.md), plan.md and tasks.md carry the amendment. The results after the fix are in the Phase 4 tables of §1 and §3.

#### D9. The JIT reads a color, normal or point destination of `attribute()`, `option()`, `rendererinfo()` and the message builtins as a float (found at T105; pre-existing; fix approved by the maintainer 2026-11-01 as T105a, no issue filed)

In `llvmEmitter.cpp` the branch for `surface`, `displacement`, `atmosphere`, `incident`, `opposite`, `attribute`, `option` and `rendererinfo` derives `resultKind` from the last letter of the matched prototype: `V` -> 3, `M` -> 16, `S` -> -1, anything else -> 1. The prototypes `f=SC`, `f=SN` and `f=SP` (color, normal, point destinations) fall in "anything else", so those calls copy one float, while the interpreter links the three to the vector overload (`DEFLINKFUNC` -> `f=SV`, three floats). Which prototype a vector argument matches depends on the order of `addBuiltInFunction`: `option` and `attribute` list `f=SV` before `f=SC`, `rendererinfo` lists `f=SC` first, so `rendererinfo("version", vector)` is compiled as a color query. Visible effect before this phase: the JIT's `rendererinfo(name, vector)` writes one float. With the `QueryType` of Phase 5 it also shows as a request that differs between the backends (Float against Triple), which the approved Phase 5 tests (`Attributes`, `RealAnswers_Attributes`, `TypeMismatch_Attribute`, `TypeMismatchReal_Attribute`, under `.slo`) assert is identical. Proposed fix (`tools/phase5-d9-emitter.diff`): C, N and P map to 3. It is the same kind of change as D8 (one emitter line, no `op_*` change); `spec.md` FR-009's amendment names the emitter only for `ctransform`, so it needed the maintainer's decision: approved 2026-11-01 and applied (FR-009 in spec.md and plan.md name it as the second emitter change). The trial changed no image on either platform.

## §5 Final summary against SC-001 to SC-014

_Filled in at the final checkpoint._

