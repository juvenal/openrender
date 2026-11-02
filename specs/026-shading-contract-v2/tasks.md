---

description: "Task list for spec 026: Shading Contract v2"
---

# Tasks: Shading Contract v2

**Input**: design documents in `specs/026-shading-contract-v2/`.

**Decisions already taken** (spec.md, Clarifications, "Session 2026-10-31"): QA to QG (the wrapper forwarding lines, the four `NoServices` tests, the 6-symbol
stub, the double's TEMPORARY block, the null object in the standalone program, a TEST-APPROVAL in every phase, `objectHandle()` in Phase 3); findings F1 to F4
resolved in this spec (FR-026 to FR-029, SC-013, SC-014), O1 to O7 filed as #115 to #121 (`validation.md` §4); analyze-stage decisions B1, B3, B4, B8 (spec.md, same session).

**Prerequisites**:
- plan.md (phases 1-10, checkpoint procedure, test design, risks)
- spec.md (US1-US7, FR-001 to FR-029, SC-001 to SC-014, the Clarifications of 2026-10-30 and 2026-10-31 including the plan-stage decisions Q1-Q14, the tasks-stage decisions QA-QG and the analyze-stage decisions B1, B3, B4, B8)
- research.md (R1-R16)
- data-model.md
- contracts/renderer-services-v2.md, tracer.md, shader-globals.md, test-doubles.md, allowlist-end-state.md
- quickstart.md (§0-§6)
- `.specify/memory/constitution.md` (Principles III and VIII), `CLAUDE.md` ("Spec-kit agent rules" and gotchas 12, 22-26), `DEVNOTES.md`,
  `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` and `RENDER_ARCHITECTURE_REFERENCES.md` (D20, D22, D25, D30)
- Precedent for the house style: `specs/028-camera-and-motion-layer/tasks.md` and its `tools/image_manifest.sh`

**Tests**: required. Constitution III (TDD) is non-negotiable. Three kinds of new test, with different red evidence:
- **Contract tests (`tests/shading_contract/`), red first by build failure** on the starting tree (the new headers do not exist), then red **at run
  time** at the start of the phase that turns each one green (the Test gate below). All of them are written in Phase 2 and approved there.
- **Replay tests (`ShadingContract_TraceReplay_*`), characterization tests** (Q7, spec 028's precedent): green on the starting tree by construction
  and shown red by a **mutation per builtin** in Phase 1. A replay file is data recorded on the starting tree and never re-recorded.
- **Tests for the findings resolved here (F1 to F4)**, written in the phase that fixes each (QF): red at that phase's Test gate by a canary overwritten, a crash on a null
  object or light, or a crossed accessor constant.
- **Registry checks** (`ctest -L architecture` after an allowlist deletion in Phases 7 and 8; `tools/scope_greps.sh` in Phase 10): red because
  the deletion or the document edit has not happened yet.

**Known-red tests (the only allowed failures)**: **none.** No test is red at any checkpoint. A test whose phase has not come is registered
`EXCLUDE_FROM_ALL` + `DISABLED` (see "Red-first convention") and shows as "Not Run (Disabled)", never as a failure. The only other non-passes
at a checkpoint are tests that skip themselves on the baseline (recorded in Phase 1), a known-flaky test that passes when re-run alone, and on Linux the
recorded Linux baseline failing set (the 7 that fail the same way on the starting tree: #44, #45, #46, #57; Phase 1 records the exact list). A Linux failure that is
not in that list is a new failure.
A test is red only between a phase's Test gate and its implementation, in the worktree, never at a checkpoint.

**Organization**: the phases are plan.md's phases 1-10, not one phase per user story, because the stories share the services interface (Phase 3
is the foundation for all of them) and each service is a separate, byte-identical increment. User-story labels (US1-US7) are on the tasks that
serve a story, the Test gate and checkpoint tasks included; baseline and Linux-handover tasks carry none. **Every phase has the same shape**:
1. a **Tests** block (enable or write the phase's tests, show them red, name every edit to an existing test);
2. an explicit **TEST-APPROVAL** pause task: the implementer stops and the maintainer approves the tests, any new reference and any edit to an
   approved test. A phase that adds and edits no test still has the task; it is a one-line confirmation the maintainer may answer in one word;
3. the **implementation** tasks, which do not start before the approval;
4. a **checkpoint** task (CP-a to CP-d, `ctest -L architecture`, Linux where `src/` changed) and a **PAUSE** for review.

**Never commit.** Stage with `git add` only, at each PAUSE. Decline any spec-kit auto-commit hook (`speckit.git.commit` before and after tasks,
implement and the other stages) and any branch hook. No GitHub writes: the issues this spec's findings called for are filed as #115 to #121 (`validation.md` §4; evidence goes to the coordinator as a comment), and #32 is closed by the maintainer's `Closes #32` line in the merge commit.

**Formatting**: run clang-format with the repo's `.clang-format` on an edited file only if it was clang-format clean before the edit (check with
`clang-format --dry-run` on the unedited file first); otherwise leave the formatting alone.

**Working directory**: `/Volumes/Projects/Development/CLI/openrender-worktrees/026-shading-contract-v2` (branch `026-shading-contract-v2`; base =
master `d01da47`, spec commit `0475e1e`). Work only in this worktree. Never touch the main checkout. Paths are repo-relative. Shell variables, as in
quickstart.md:

```bash
W=/Volumes/Projects/Development/CLI/openrender-worktrees/026-shading-contract-v2
T=$W/specs/026-shading-contract-v2/tools
CJ=~/.claude/bin/cpujob
H=juvenal@ubuntu-2404-desktop.v2-labs.io            # always "${H}:path", never $H:path (zsh's :P modifier copies locally and exits 0)
RR=Projects/Development/CLI/openrender-026          # the remote per-spec tree, relative to the remote home
```

Build trees (all gitignored by `build-*`): `build` (macOS, Release, JIT on, configured at the starting tree), `build-logs/` (job logs), `build-red`
(scratch, Phase 2), `build-nojit` (scratch, created at the Phase 3 checkpoint for the JIT-off compile and reused by Phase 10), `build-base` (the starting tree for SC-010, Phase 10).

---

## Standing rules (apply to every task)

**Builds and tests (marked [RUN])**:
- The implementer asks the coordinator before each [RUN], then runs it through `~/.claude/bin/cpujob <label> <command>`: one job at a time
  machine-wide, `nice -n 19`, at most `-j4` on macOS; the determinism sweep and every Linux job run at `-j2`.
- Long jobs go in the background with output redirected to `build-logs/<label>.log`. Wait for the completion notice. Exit 124 is a timeout:
  report it, do not retry blindly (a first Linux build may need `CPUJOB_TIMEOUT=7200`; ask).
- Never run a test binary directly (a hung one has no timeout). Never use `-j8`, a bare `-j`, or a count above 4.
- A plain `cmake --build` / `ctest` line below always means the `cpujob` form.
- The Linux machine is reached by the implementer over ssh (the maintainer's decision, Q13), in its own tree, with the remote `cpujob`. See "Linux".
- A `cmake -B` configure can trigger a vcpkg build, so every configure runs through `cpujob` too (`$CJ 026-<what>-configure cmake -B ...`; on Linux the remote form below).

**Test gate (TG)**: how every phase from 3 on starts. Defined once, here.
1. Remove `EXCLUDE_FROM_ALL` and `DISABLED` from the tests the phase turns green (named in the phase's Tests block) and from nothing else.
2. [RUN] build their targets, then run them (`ctest -R '<names>' --output-on-failure`). They must be **red for the stated reason** (an assertion
   naming the missing v2 request, or, for an allowlist phase, the `VIOLATION` lines). A test that is already green is a defect in the test: stop.
3. Record each test's failing output in `validation.md` §2.
4. Apply only the edits to existing tests that the phase's Tests block lists (see "Edits to existing tests"), and name each in the approval task.
5. Stage, summarize, **stop for TEST-APPROVAL**. No `src/` edit before the approval.

**Edits to existing tests** (FR-023 as amended by Q3, Q7 and QA):
- Allowed: deleting the overrides of the 28 removed methods and changing `setOffendingObject`'s parameter type in the four whole-interface classes
  (`CForwardingServices` in `tests/shading_parity/table_services.h`; `CRecordingServices` and `CStubServices` in `recording_services.h`;
  `CTextureServices` in `recording_texture_system.h`); trailing fields in the two spec-039 observer reports; include paths and the doubles' base class.
- **Allowed by QA (decided 2026-10-31, FR-023 amended); each is still named at its phase's TEST-APPROVAL**: a forwarding override of `getMatrix` (Phase 4), `getAttribute`
  (Phase 5), `getMessage` (Phase 6) and `tracer()` (Phase 8) in the wrappers that forward to an inner implementation (`CForwardingServices`,
  `CRecordingServices`, `CTextureServices`; `CStubServices` answers fixed sentinels and gets none). Without it the new members have their `Unavailable` defaults in
  those wrappers, and an existing test that installs one would silently lose coordinate systems, options or traces. The tests that install a wrapper are the six in `tests/shading_parity`:
  `test_context_no_services.cpp`, `test_state_services.cpp`, `test_services_routing.cpp`, `test_service_lookup.cpp`, `test_texture_substitute.cpp` and `test_texture_substitute_3d.cpp`; the last two are the likely users
  (`texture3d`'s coordinate systems, ray-traced `environment`/`shadow`). Each phase's checkpoint runs all six unchanged, which is the evidence for or against the override.
- Any other edit to a test, a threshold, a reference or a replay file is not allowed. A compile fix to Phase 2 test code made necessary by the
  real headers (Phase 3) is listed at that phase's approval with its diff.

**Checkpoint procedure** (referenced as CP-a to CP-d by every phase's last task; commands as in quickstart.md §2). Defined once, here:
- **CP-a build**: `$CJ 026-pN-build cmake --build $W/build --config Release -j4`. It must be clean under `-Werror`. It builds every target, ribVector included.
- **CP-b suite**: stamp first (`$T/image_manifest.sh stamp $W/build`), then
  `$CJ 026-pN-suite ctest --test-dir $W/build -LE determinism-sweep -j4 --output-on-failure`, then copy the log before the next ctest run overwrites
  it: `cp $W/build/Testing/Temporary/LastTest.log $W/build-logs/pN.LastTest.log`.
  - Expected: the baseline pass set (counted in Phase 1) plus the tests this spec has enabled so far. No known-red set.
  - A red test from the known-flaky set (memory `reference_ctest_flaky_tests.md`: `ImagerRegressionRender`, `Parity_transparency-matte-aov`,
    `Parity_depthdefault`, `ShadingParity_AmbientAccumulation`) is re-run alone before it is written off. `ShadingParity_AmbientAccumulation` is a
    real heap bug, never noise: re-run it under the baseline's conditions and compare.
  - The suite includes `^determinism$` (the default 148), `visual`, `shading_parity`, `texture_tile` and `architecture`.
- **CP-c architecture**: `$CJ 026-pN-arch ctest --test-dir $W/build -L architecture --output-on-failure`, plus the full report
  `$CJ 026-pN-archreport $W/build/tests/architecture/archcheck --source-dir $W --build-dir $W/build --config $W/tests/architecture`. Record the
  `ALLOWED`, `METRIC` and `RESULT` lines in validation.md §1. **This run is part of every phase report**, Phases 1 and 2 included. Expected
  `ALLOWED`, cumulative (macOS and Linux alike):

  | After phase | DB-001 | DB-005 | DB-006 | Allowlist lines deleted so far |
  |---|---|---|---|---|
  | 1 to 6 | 8 | 8 | 33 | 0 |
  | 7 | 8 | 8 | 31 | 2 |
  | 8, 9, 10 | 5 | 8 | 31 | 5 |

  - Expected otherwise: `RESULT PASS`, no `STALE`, no `VIOLATION`, no `BADID`, no unclassified file, no "incomplete build tree" exit (archcheck
    walks every `DependInfo.cmake`, `EXCLUDE_FROM_ALL` targets included; research R16 of spec 028). The metric rows (Shading 0/0) do not rise.
  - The new sources sit in directories `domain-map.txt` already classifies; the new edges (Shading -> State, Hiders -> Shading, Core -> Shading and
    Hiders) are allowed ones.
- **CP-d byte-identical output** (every phase from 3 on, for **all** suite images): after CP-b and before any other ctest run,
  `$T/image_manifest.sh diff $T/baseline-images-macos.sha256 $W/build $T/baseline-image-noise-macos.txt` must report **0 differing and 0 missing
  images**, and `$T/image_manifest.sh diffs $W/build-logs/pN.LastTest.log | diff - $T/baseline-diffs-macos.txt` must be empty (every
  `MaxBlockAvgDiff` equal). A noise-listed image's `MaxBlockAvgDiff` must still match. Any difference stops the phase: it is a behaviour change in a
  behaviour-preserving phase (FR-022). **No phase of 026 changes an image.**
- **CP-m contract tests**: `$CJ 026-pN-contract ctest --test-dir $W/build -L '^shading_contract$' --output-on-failure` (anchored: `-L` is a
  regular expression, and an unanchored `shading_contract` also selects `shading_contract-record`). Expected: every enabled test green, every
  later test "Not Run (Disabled)".
- **CP-L Linux** (every phase that changes `src/`, Phases 3 to 10; Phase 1 takes the baseline): see "Linux".
- **Per platform**: macOS runs CP-a to CP-d at every phase from 3 on; Linux runs them against its own `-linux` baseline files. There is no
  cross-platform bit comparison.

**Linux (GCC 13, `-Werror`)**: the implementer, over ssh, in the per-spec tree `~/Projects/Development/CLI/openrender-026`, with the remote
`~/.claude/bin/cpujob` at `-j2`. **Never touch `~/Projects/Development/CLI/openrender` or `openrender-039`.** The sync is the maintainer's recipe:
- `$T/linux_sync.sh push` (written in Phase 1) builds the file list with `git ls-files -z --cached --others --exclude-standard`, dropping entries
  that no longer exist on disk. It uses `--others` because a new file is untracked until `git add`, and `git ls-files` alone would omit it. Then
  `rsync -rlpc --from0 --files-from=<list> ./ "${H}:$RR/"`, with `tests/shading_contract/replay/linux.txt` and `specs/026-shading-contract-v2/tools/*-linux*`
  excluded (they are Linux-only; the remote copy is the one Linux reads). Files that an earlier push sent and the list no longer holds are removed
  from the remote, using the remote's `.sync-manifest` of the previous push, never a `--delete`.
- `$T/linux_sync.sh verify` compares `shasum -a 256` of every listed file locally with `sha256sum` remotely, and fails on any difference or on a
  remote `.git`. **Run `verify` after every push and after every pull, and record "N files, 0 differences" in validation.md §3.**
- `$T/linux_sync.sh pull` copies the Linux-only files back (`tests/shading_contract/replay/linux.txt`, `specs/026-shading-contract-v2/tools/*-linux*`)
  with `rsync -rlpc` and verifies them the same way. Linux-only files are copied back, never pushed.
- The script hard-codes the remote root and refuses any other. A remote command is `ssh "$H" "cd $RR && ~/.claude/bin/cpujob 026-lN-<what> <command> -j2"`.
- The remote tree has no `.git`; nothing in the build may depend on git.
- **CP-L** = push, verify, build (CP-a), the suite with a fresh stamp (CP-b), CP-d against the `-linux` baseline files, CP-c, CP-m. Linux images are
  compared only with Linux's own baseline. The record of each run goes in validation.md §3.

**Allowlist rules (FR-019)**: allowlist lines are identified by **file and symbol (the key), never by line number**: the numbers in research and the
contract (`:15, :16, :21, :88, :89`) are `d01da47`'s and can shift. The only allowlist change in 026 is **deleting these 5 lines**, each at the start of
the phase that names it (its Test gate), plus the two section-header counts:
- Phase 7, DB-006 include: `execute.cpp -> src/ri/geometry/surface.h`; `executeMisc.cpp -> src/ri/geometry/surface.h`.
- Phase 8, DB-001 link: `executeMisc.cpp -> _ZN15CShadingContext5traceEP4CRay`; `executeMisc.cpp -> _ZN15CShadingContext7traceExEP10CRayBundle`;
  `shading.cpp -> _ZN15CShadingContext7traceExEP10CRayBundle`.
- `shading.cpp -> src/ri/geometry/object.h` **stays** (spec 029). DB-005 and DB-018 have no deletion.
**Adding, re-keying or swapping a line is never allowed.** After a deletion, `ctest -L architecture` must report exactly that phase's edges as
`VIOLATION` and nothing else; any other violation means the design is wrong: stop and report. `metrics-baseline.txt` only goes down, and only to
values the check prints.

**Thresholds and references**: never widen a threshold, never disable a test (other than the red-first `DISABLED` markers below, each removed by the
phase that provides the behaviour), never regenerate a reference image, never re-record a replay file from a changed tree, never edit an approved
test without the approval task, and never add a path to the noise list to get a green result. A new reference (the two replay files) needs the
maintainer's approval at the pause that records it (`T043`).

**Red-first convention**: a contract test is added with `EXCLUDE_FROM_ALL` on its executable and `set_tests_properties(<name> PROPERTIES DISABLED TRUE)`
on its test, so the main build and every checkpoint stay green.
- The CMake option `OPENRENDER_SHADING_CONTRACT_TESTS` (in `tests/shading_contract/CMakeLists.txt`) gates the contract programs. It is **OFF until
  Phase 3**, when the headers exist; Phase 3 turns it ON by default. From then on only the per-test `DISABLED` markers hold the later tests back.
- The Phase 2 red evidence is taken in a scratch build with the option ON: `-DOPENRENDER_SHADING_CONTRACT_TESTS=ON` (without it the two targets
  would not exist and the "failure" would mean nothing).
- **A case that calls a Shading member whose signature changes in a later phase** (`attributes()`/`options()`, `surfaceParameter()` and the other four
  parameter members, `setOffendingObject`) gets an executable of its own, `EXCLUDE_FROM_ALL` until that phase, exactly as `Situations` does (B10): in the
  shared rendered program it would stop every other rendered test from compiling. Its red evidence is the build failure, recorded at the Test gate. (Exception, N1: the Phase 6 message family compiles from Phase 5, because Phase 5 gives the five members their `QueryType` argument; its red evidence is the overwritten canary.)
- The replay tests (Phase 1) compile no new header and need no option; they are `DISABLED` only until the hook that records them exists.
- A test that is split by service is registered once per service so each can be enabled in its own phase (`NoServices` is four tests; each
  `RealAnswers` test is registered per service).

**Mutation check**: every characterization test gets a mutation at its green point (edit, rebuild, show the named test red, revert, confirm
`git diff -- <file>` empty (the hook is staged, Phase 1); results in validation.md §2). Phase 1 does the eight replay mutations.

**Hand-offs to the maintainer** (the implementer neither does nor fakes these):
- **Filing issues.** The findings were triaged at the tasks stage (`validation.md` §4): F1 to F4 are resolved by this spec's tasks, tests first; O1 to O7 are
  out of scope and **filed as #115 to #121** (P1, `priority: high`; §4.3 keeps the text). The implementer adds evidence as a comment on the issue if the code shows more
  (a repro run, a line number that moved) but does not write to GitHub itself: it hands the comment text to the coordinator.
- **SC-012's review "by someone who did not write it"**: Phase 10 asks the maintainer to name the reviewer and records the answer; the implementer
  does not review its own header.
- **`CLAUDE.md`**: edited only with the maintainer's explicit authorization (Phase 10 asks), and never its "Spec-kit agent rules" section.

**No task depends on the merge**: no post-merge step, no merge-commit hash, no GitHub comment. Registry entries DB-001, DB-005, DB-006 and DB-018
stay **Open**; none moves to Resolved. Their text records "spec 026" and the date the task is done.

**Files 026 never edits** (plan "Not edited"; `git diff --stat 628d6747 -- <files>` stays empty, checked in Phase 10):
`src/ri/hiders/trace.cpp`, `bundles.{h,cpp}`, `photonMap.*`, `raytracer.*`, `reyes.*`, `stochastic.*`, `zbuffer.*`, `photon.cpp` (the observer
and the tracer helpers are called from Shading and from `irradianceDispatch.cpp` only); every Geometry, Graphics State and Texturing source; the JIT
emitter (`src/libshader/compiler/llvmEmitter.cpp`) and every `op_*` signature (the two exceptions, decided by the maintainer: T086a, the two-space `ctransform` and `op_ctransform_ss`, D8, 2026-10-31; T105a, the triple `resultKind`, D9, 2026-11-01); the plug-in ABI headers (`shadeop.h`, `dlo.h`, `implicit.h`,
`dsply.h`); `CLAUDE.md` unless authorized.

**Stop conditions** (stop and report, never work around):
- any CP-d difference, any `MaxBlockAvgDiff` difference, any replay difference, or an unexpected `VIOLATION` in a phase from 3 on (plan "Stop rule");
- a new header would need an include from Hiders, Geometry or the renderer core (FR-003), or a Shading source would need `renderer.h`;
- archcheck exits "incomplete build tree" once the contract programs are on;
- a replay mutation does not turn its family's test red (the replay is too weak: strengthen the scene in Phase 1);
- a Phase 4-8 test is green at its Test gate (the test proves nothing);
- master (or another branch) is merged into this branch while the work is in progress: the baselines are of `d01da47`, so the maintainer decides;
- a Linux-only starting-tree failure, or GCC 13 `-Werror` rejecting test code the maintainer approved;
- a CP-d difference caused by F1, F2 or F3 (a scene reached a path that overflowed, mixed a string with a number or dereferenced a null object or light on master:
  the maintainer decides; nothing is widened);
- the static re-measurement (Phase 1) differs from the plan's numbers in a way that changes a design decision (the 57 methods, the 25 coordinate call
  sites counted as `T008` defines them, 10 + 2 trace call sites, the 6 imports).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency on unfinished tasks).
- **[Story]**: US1 (Shading has one documented, versioned, test-doubled boundary), US2 (renders do not change), US3 (message passing and attribute
  queries go through the services), US4 (coordinate systems come from one service), US5 (a shading point has a contract with a ray type and hit mode),
  US6 (tracing goes through a Shading-owned interface), US7 (the registry reflects what moved).
- **[RUN]**: a build or test run done by the implementer, one job at a time, after asking the coordinator.
- **TEST-APPROVAL / PAUSE**: the implementer stops; the maintainer replies.

---

## Phase 1: Setup, baselines, the trace replay and the Linux gate (before ANY other `src/` edit)

**Goal**: the starting tree's macOS and Linux baselines, the null-in-production `traceRequestObserver` at the 10 + 2 call sites, the replay recorded
on the starting tree and shown red under eight mutations. **Gate**: Phase 2 does not start until the Linux files exist (after Phase 3 begins the
starting `src/` can no longer be built with 026's tests, so the baseline cannot be taken later).

### Setup and static baselines

- [X] T001 Orient in the worktree and read the rules. Run `git status` and `git log -1` (expect branch `026-shading-contract-v2`, base `d01da47`, spec
  commit `0475e1e`, only `specs/026-shading-contract-v2/` untracked or modified), `cat .specify/feature.json` (`specs/026-shading-contract-v2`),
  `git check-ignore build build-red build-nojit build-base build-logs` (all five must print: `.gitignore` has `build-*`), and `df -h /Volumes/Projects`
  (7.5 GiB were free at the plan stage; a build tree is about 0.6 GiB, and three scratch trees are used one at a time). Read `CLAUDE.md` (the worktree
  copy and the main checkout's copy are identical, commit time 1793332971; the "Spec-kit agent rules" section applies), `DEVNOTES.md`,
  `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`, constitution Principles III and VIII, and spec 028's `tasks.md` header. Record the output in validation.md §0.
- [X] T002 [P] Create `specs/026-shading-contract-v2/tools/` and copy `specs/028-camera-and-motion-layer/tools/image_manifest.sh` into it
  **unchanged** (`cp -p`; quickstart §0; 028's copy is not edited). Check it is executable and that `sha256sum`/`shasum` is found.
- [X] T003 [P] `specs/026-shading-contract-v2/validation.md` already exists (seeded at the tasks stage with §4, the finding triage and the text of the seven issues filed as #115 to #121); do not recreate it. Fill it
  as the work proceeds: §0 starting-tree measurements; §1 checkpoint records (one table per phase: CP-a to CP-d, CP-m, `ALLOWED`/`METRIC`/`RESULT`); §2 red evidence (Test gates, mutations, the red
  output of the F1 to F4 tests); §3 Linux records (every push and `verify`, builds, suites, baselines); §4 only gains evidence (a repro that was run, a moved line number), which goes to the coordinator as a comment on the issue, and a
  "closed by T-number" note against F1 to F4; §5 final summary against SC-001 to SC-014.
- [X] T004 [US2] [RUN] macOS baseline build and suite at the **starting `src/`** (no source edit yet): `$CJ 026-base-configure cmake -B build -S . -DCMAKE_BUILD_TYPE=Release`
  (through `cpujob`: a configure can trigger vcpkg; use the vcpkg environment the coordinator gives for this machine; do not invent `VCPKG_ROOT`; the JIT is ON), `$CJ 026-base-build cmake --build build
  --config Release -j4`, `$T/image_manifest.sh stamp build`, `$CJ 026-base-suite ctest --test-dir build -LE determinism-sweep -j4 --output-on-failure`,
  `cp build/Testing/Temporary/LastTest.log $T/base-LastTest-macos.log`, `$T/image_manifest.sh record build > $T/baseline-images-macos.sha256`,
  `$T/image_manifest.sh diffs $T/base-LastTest-macos.log > $T/baseline-diffs-macos.txt`. Record the test count, the pass set, and any test that skips
  itself or fails on the starting tree (a failing one is a stop condition for the maintainer, not a baseline to hide).
- [X] T005 [US2] [RUN] Noise check: stamp and run the suite a second time; list every image whose bytes differ between the two runs in
  `$T/baseline-image-noise-macos.txt` (spec 028 found two `shading_parity` scratch files). Check that a noise-listed image's `MaxBlockAvgDiff` is equal in both runs.
- [X] T006 [US7] [RUN] Architecture report and imports on the starting tree: `$CJ 026-base-archreport build/tests/architecture/archcheck --source-dir .
  --build-dir build --config tests/architecture > $T/archcheck-base-macos.txt`; `nm -m build/src/libshader/shading/libshader_shading.dylib | grep 'dynamically
  looked up' | c++filt > $T/libshader-imports-base-macos.txt`; `dyld_info -fixups build/src/libshader/shading/libshader_shading.dylib | c++filt >
  $T/libshader-fixups-base-macos.txt`. Expected: `ALLOWED DB-001 x 8`, `DB-005 x 8`, `DB-006 x 33`, `RESULT PASS`, and the 6 imports of research R10
  (`CPhotonMap::lookup`, `CGatherBundle::CGatherBundle()`, `CShadingContext::trace(CRay*)`, `CShadingContext::traceEx(CRayBundle*)`, vtable for
  `CTraceBundle`, vtable for `CTransmissionBundle`). Confirm from `dyld_info` that the dylib uses chained fixups that bind every import at load.
- [X] T007 [US2] [RUN] SC-007 holds for the determinism sweep too (B1), so record its image manifest on the **starting tree**, before `T017`: `$T/image_manifest.sh stamp build`, then `$CJ 026-base-sweep ctest --test-dir build -L
  '^determinism-sweep$' -j2 --output-on-failure` (413 scenes x 5 configurations, about 10 to 12 minutes at `-j2`), `cp build/Testing/Temporary/LastTest.log $T/base-sweep-LastTest-macos.log`, `$T/image_manifest.sh record build >
  $T/baseline-sweep-images-macos.sha256`. Run it a second time and list the paths whose bytes differ between the two runs in `$T/baseline-sweep-noise-macos.txt`. If the sweep leaves no image under `build/tests` the manifest is empty: then record the
  pass/fail set and whatever output the sweep keeps, say so in validation.md §0, and ask the coordinator what the final comparison (`T176`) should diff.
- [X] T008 [US1] Re-take the plan's static counts on this worktree (no build) and record them in validation.md §0 with the current line numbers
  (the plan's line numbers are `d01da47`'s; later tasks locate sites by symbol and by `grep`, not by number): the 57 pure virtuals of `CRendererServices`
  and the 57 overrides in `src/ri/render/rendererServicesImpl.h`; the `findCoordinateSystem` call sites, **counted as textual calls to
  `findCoordinateSystem(` or `jitFindCoordinateSystem(` outside their declarations, definitions and comments**: expected 25 (17 in `shaderFunctions.h`, 1 in `giFunctions.h`, 1 in
  `shaderOpcodes.h`, 3 in `shading.cpp` (`:3532, :3644, :3684`), 3 in `rslOps.cpp` through `jitFindCoordinateSystem`) plus the 2 forwarders inside `shading.cpp` (the 3-argument overload and
  `jitFindCoordinateSystem`'s own call), not the 28 that earlier text summed; the spec's "about 30" counts mentions; the 10 Shading trace call sites
  (`executeMisc.cpp`: `trace(` at the probe forms and the reflection probe, `traceEx(` six times in `traceTransmission` and `traceReflection`; `shading.cpp`:
  `traceEx(lastGather)` twice in `gatherSample`) plus the 2 in `src/ri/hiders/irradianceDispatch.cpp`; the reads of `currentObject->attributes`
  (`grep -rn 'currentObject->attributes' src/libshader`: the plan's "17" is a figure to re-measure, and the tree shows 22 code occurrences; **a differing total is not a stop condition**, the 7 + 1 sites below are the figure that matters) and, of the 8 sites compiled into `execute.cpp`/`executeMisc.cpp`/`shaderFunctions.h`/`giFunctions.h`, the **7 that read the object's
  attributes plus the 1 (`scripterror`) that passes the object pointer to `setOffendingObject`**; the five allowlist lines to be deleted
  (by key). Any difference that changes a design decision is a stop condition.
- [X] T009 [P] [US5] Grep the hit-mode values the visual suite uses: `grep -rhoE '(camerahitmode|specularhitmode|transmissionhitmode|diffusehitmode)"?[^"]*"[^"]*"'
  tests/visual --include='*.rib' | sort | uniq -c`. Record the (attribute, value) pairs in validation.md §0. Phase 2's `Situations` scenes must cover every
  (call site, hit mode) pair found (research R13, SC-006), **rows 6 and 7 excepted (classify-only, B8)**: a pair that only rows 6 or 7 reach (implicit-surface and DSO-object hits) is asserted from `classifyShading()`, not rendered.
- [X] T010 [P] Write `specs/026-shading-contract-v2/tools/linux_sync.sh` with the subcommands `push`, `verify`, `pull` and a `--dry-run` that prints the file list
  and runs nothing remote, exactly as the standing rule "Linux" specifies (list from `git ls-files -z --cached --others --exclude-standard` minus
  entries missing on disk; `rsync -rlpc --from0 --files-from`; the two Linux-only exclusions; removal of files that a previous push sent and the list
  no longer holds, via the remote `.sync-manifest`; `verify` by `shasum -a 256` against remote `sha256sum`, failing on any difference or on a remote
  `.git`; remote root hard-coded to `Projects/Development/CLI/openrender-026`, refusing any other, and refusing a path that ends in `/openrender` or
  `/openrender-039`; the form `"${H}:path"` everywhere). Run `--dry-run` and check that the list holds no `build*` path and no `.git`.

### Tests block: the trace replay (characterization tests, US6)

- [X] T011 [US6] Create `tests/shading_contract/CMakeLists.txt` and add `add_subdirectory(shading_contract)` to `tests/CMakeLists.txt`. In it: declare
  `option(OPENRENDER_SHADING_CONTRACT_TESTS "Spec 026 contract programs" OFF)` (Phase 2 fills it), and the replay targets below, every executable
  `EXCLUDE_FROM_ALL` and every test `DISABLED TRUE` until `T021`. Link `ribRender libshader_shading openrendercommon openrender_common_flags`
  like `tests/shading_parity`. Labels: `shading_contract` for the checks, `shading_contract-record` for the recorder. Environment as in
  `tests/shading_parity/CMakeLists.txt` (`SHADERS`, `ORENDERHOME`, `OSHADER_BIN`). `TIMEOUT` on every test.
- [X] T012 [P] [US6] Write `tests/shading_contract/replay/trace_replay_record.cpp`: renders the fixed scene set in process, **one render thread** so the order is
  defined, once per backend (`.rslo`; `.slo` when the JIT is on), installs `CShadingContext::traceRequestObserver` (declared in `T017`), and writes
  `replay-fresh.txt` into the build tree only. One record per request: kind, label, bundle depth, path depth, then per ray `from`, `dir`, `t`, `tmin`,
  `time`, `flags`, `da`, `db`, `key`, `depth`, floats as hex floats (research R14). Section headers name the scene and backend. Test
  `ShadingContract_TraceReplayRecord` (label `shading_contract-record`).
- [X] T013 [P] [US6] Write `tests/shading_contract/replay/trace_replay_check.cpp`: given a builtin name, re-renders that builtin's scenes (both backends) and
  compares the fresh requests with `tests/shading_contract/replay/macos.txt` or `linux.txt` (chosen at compile time by `__APPLE__`) **exactly**. A
  missing replay file fails with "no replay file" (spec 028 found this on Linux); an empty section fails. The failure message names the builtin, the
  scene, the request index and the first differing field. Tests `ShadingContract_TraceReplay_{trace,transmission,visibility,gather,environment,shadow,
  occlusion,indirectdiffuse}` (8).
- [X] T014 [P] [US6] Write the eight scenes `tests/shading_contract/replay/scenes/replay_<builtin>.rib` and their shaders `.../shaders/replay_<builtin>.sl`: small (a few
  pixels), raytrace hider, fixed `Option "sampling"` sampler and seed (spec 039), no motion, each builtin used with at least two distinct shapes of
  call (for example `trace` with and without a bias and a `samples` parameter; `transmission` and `visibility` with a bundle form and a probe form;
  ray-traced `environment`/`shadow` through `"raytrace"`; `occlusion` and `indirectdiffuse` with the irradiance cache). Compile each shader to `.rslo` and
  (JIT on) `.slo` as build steps through `oshader`/`oshader --jit`, as `shaders/CMakeLists.txt` does. Every family must produce at least one request in each backend.
- [X] T015 [US6] [RUN] Red evidence by build failure: with the replay targets enabled only in a scratch invocation (`cmake --build build --target
  test_shading_contract_trace_replay_record`, after temporarily removing `EXCLUDE_FROM_ALL`), the build fails naming `traceRequestObserver`. Paste the
  error in validation.md §2, then restore the markers.
- [X] T016 [US6] **TEST-APPROVAL (PAUSE 1a)**: stage `tests/shading_contract/`, `tests/CMakeLists.txt`, `tools/linux_sync.sh`, `tools/image_manifest.sh`, the
  baselines and `validation.md`. Summarize: the recorder's format, the eight scenes and what each exercises, the eight planned mutations (below), why
  these are characterization tests (green by construction, red by mutation, Q7), and the T-numbers of the baselines. **Wait for the maintainer's
  approval of the replay tests and of the mutation list.** Nothing after this task edits `src/` until it is answered.
  **Answered 2026-10-31 (maintainer, via the coordinator): approved, every recommendation accepted.** (1) The replay design: one child process per scene (GitHub #2), 4x4 pixels, raytrace hider, 1 thread, sobol sampler with seed 1, a discarding display, the line format with hex floats. (2) The eight scenes, including the probe forms that vary by direction and distance (the float `trace` and `visibility` forms take no parameter list). (3) The hook shape: the public `CShadingContext::reportTraceRequest(kind, label, bundleDepth, numRays, rays)` member and an `if (traceRequestObserver != nullptr)` guard at each of the 12 sites. (4) The eight mutations as listed, each reverted and followed by a check that `git diff` is clean. (5) **Note for Phase 2: T009 found no hit-mode attribute in the visual suite, so the `Situations` scene set (T044 and the scenes it names) must cover the non-default hit modes itself.**

### The hook (null in production; a source change that must not change any image)

- [X] T017 [US6] In `src/libshader/shading/shading.h` declare, next to `CPointKeyReport`/`pointKeyObserver`, a `CTraceRequestReport` (kind, label, bundle depth,
  path depth, ray count, a pointer to the rays) and `typedef void (*CTraceRequestObserver)(const CTraceRequestReport &)`, and
  `static CTraceRequestObserver traceRequestObserver;` with the comment "test hook, null in production; set only by tests" (spec 039's precedent). Define it
  `= nullptr` in `shading.cpp` beside `pointKeyObserver`. The kind enum names the eight builtins' families. Nothing is allocated unless the observer is set.
- [X] T018 [US6] In `src/libshader/shading/executeMisc.cpp`, just before each of the 8 trace calls (the probe `trace(` of the transmission/visibility forms, the
  reflection probe `trace(`, and the six `traceEx(` in `traceTransmission`/`traceReflection`; locate by `grep -n 'trace(\|traceEx(' executeMisc.cpp`), add
  `if (traceRequestObserver != nullptr) { ... }` reporting the ray or the bundle's rays, label and depth. No other edit in the file.
- [X] T019 [US6] In `src/libshader/shading/shading.cpp`, the same before the two `traceEx(lastGather)` calls in `gatherSample` (shared by the interpreter and the JIT).
- [X] T020 [P] [US6] In `src/ri/hiders/irradianceDispatch.cpp`, the same before the two `context->trace(&ray)` calls in `CIrradianceCache::sample` (kinds `occlusion` and `indirectdiffuse`).
  This is a Hiders file; the call is to a Shading static, which the table allows. No other edit.
- [X] T021 [US6] [RUN] Remove `EXCLUDE_FROM_ALL` from the recorder and the checker (the 8 tests and the recorder stay `DISABLED` until `T023`), then
  CP-a: `$CJ 026-p1-build cmake --build build --config Release -j4`. Clean under `-Werror`; `ribVector` builds.
- [X] T022 [US2] [RUN] Prove the hook adds no output: CP-b (stamp, suite, copy the log as `p1.LastTest.log`), then `$T/image_manifest.sh diff
  $T/baseline-images-macos.sha256 build $T/baseline-image-noise-macos.txt` must exit 0 and `$T/image_manifest.sh diffs build-logs/p1.LastTest.log | diff -
  $T/baseline-diffs-macos.txt` must be empty. Any difference stops the phase. Once the images match, `git add` the hook files (`src/libshader/shading/shading.h`, `shading.cpp`, `executeMisc.cpp`, `src/ri/hiders/irradianceDispatch.cpp`) so that from here on `git diff` (unstaged) shows only a mutation.
- [X] T023 [US6] [RUN] Record: enable `ShadingContract_TraceReplayRecord` only and run it **twice** (`$CJ 026-replay-rec ctest --test-dir build -R
  ShadingContract_TraceReplayRecord`, `-L shading_contract-record` selects the same); the two `build/tests/shading_contract/replay-fresh.txt` must be
  byte-identical (the replay is deterministic). Copy one to `tests/shading_contract/replay/macos.txt`. Record in validation.md §2 the request count per
  family and backend; a family with no requests in either backend is a defect in its scene (fix the scene, which re-opens `T016`).
- [X] T024 [US6] [RUN] Remove `DISABLED` from the eight `TraceReplay_*` tests, build, run `-L '^shading_contract$'`: all eight green by construction.
- [X] T025 [US6] [RUN] Mutation `trace`: in `traceReflection`'s ray setup in `executeMisc.cpp` drop the bias (set `tmin` to 0 for the reflection bundle's rays), rebuild,
  run `ShadingContract_TraceReplay_trace`: it must be red naming the field. Record the exact edit and the failing line in validation.md §2. Revert, and
  confirm `git diff -- src/libshader/shading/executeMisc.cpp` is empty (the hook is staged, so any unstaged difference is a leftover mutation).
- [X] T026 [US6] [RUN] Mutation `transmission`: in `traceTransmission` swap the order in which the exterior and interior bundles are traced (or change the copied `time`),
  rebuild, `ShadingContract_TraceReplay_transmission` must be red. Record, revert, confirm.
- [X] T027 [US6] [RUN] Mutation `visibility`: in the probe form used by `visibility()` change `tmin` (set it to 0), rebuild,
  `ShadingContract_TraceReplay_visibility` must be red. Record, revert, confirm.
- [X] T028 [US6] [RUN] Mutation `gather`: in `gatherSample` (`shading.cpp`) zero the ray footprint `da` of the gather rays, rebuild,
  `ShadingContract_TraceReplay_gather` must be red. Record, revert, confirm.
- [X] T029 [US6] [RUN] Mutation `environment`: in the ray-traced `environment()` setup (`giFunctions.h`, the path that reaches `traceReflection`) change the bias
  passed to the trace, rebuild, `ShadingContract_TraceReplay_environment` must be red. Record, revert, confirm.
- [X] T030 [US6] [RUN] Mutation `shadow`: in the ray-traced `shadow()` setup (`giFunctions.h`, the path that reaches `traceTransmission`) change the bias, rebuild,
  `ShadingContract_TraceReplay_shadow` must be red. Record, revert, confirm.
- [X] T031 [US6] [RUN] Mutation `occlusion`: in `irradianceDispatch.cpp`'s occlusion ray (`CIrradianceCache::sample`) change `tmin`, rebuild,
  `ShadingContract_TraceReplay_occlusion` must be red. Record, revert, confirm.
- [X] T032 [US6] [RUN] Mutation `indirectdiffuse`: the same for the indirect-diffuse ray, `ShadingContract_TraceReplay_indirectdiffuse` must be red. Record, revert, confirm.
  If any of the eight mutations leaves its test green, stop: the scene is too weak (strengthen it, which re-opens `T016`); a mutation that is bitwise-equivalent
  may be replaced by a named one only with the maintainer's approval.
- [X] T033 [US6] [RUN] After the eight reverts: `git diff` (unstaged) is empty, rebuild, run `-L '^shading_contract$'` (all eight green) and
  repeat `T022` (CP-b and CP-d) once more, since the tree was rebuilt eight times.
- [X] T034 [US7] [RUN] CP-c on macOS: expect `ALLOWED DB-001 x 8`, `DB-005 x 8`, `DB-006 x 33`, `RESULT PASS`, and the Shading metric unchanged
  (`irradianceDispatch.cpp` adds a Hiders -> Shading reference, which is allowed). Record in validation.md §1 as the baseline for the cumulative table.

### Linux baseline (the gate, quickstart §4a)

- [X] T035 [RUN] Linux 1: `$T/linux_sync.sh push`, then `$T/linux_sync.sh verify` (record "N files, 0 differences"), then `ssh "$H" "ls $RR/specs/026-shading-contract-v2 &&
  test ! -e $RR/.git && echo no-git-ok"`, then configure through the remote `cpujob`: `ssh "$H" "cd $RR && ~/.claude/bin/cpujob 026-l1-configure cmake -B build -S . -DCMAKE_BUILD_TYPE=Release"` with the Linux flags the coordinator gives.
- [X] T036 [RUN] Linux 2: `ssh "$H" "cd $RR && ~/.claude/bin/cpujob 026-l1-build cmake --build build --config Release -j2"` (GCC 13, `-Werror`). A GCC-only error in the
  hook or the replay test code is fixed here and the macOS build re-run; one in code the maintainer already approved is a stop condition.
- [X] T037 [US7] [RUN] Linux 3: `ctest --test-dir build -L architecture` on the remote, saving the full report as `$RR/specs/026-shading-contract-v2/tools/archcheck-base-linux.txt`. Expect
  the same 8 / 8 / 33 allowed lines and the same five deletion keys present (no platform tag).
- [X] T038 [US6] [RUN] Linux 4, **before the suite**: `ctest -L shading_contract-record` twice on the remote (the two `replay-fresh.txt` byte-identical), copy one to
  `$RR/tests/shading_contract/replay/linux.txt`, then `-L '^shading_contract$'` (the 8 replay tests green). Without that file the replay tests fail with "no replay file".
  **First attempt (2026-10-31): the two Linux recordings were NOT byte-identical; the `.slo` sections of `transmission` and `shadow` differ run to run (validation.md D1). No `linux.txt` was kept; the evidence is in `tools/replay-nondeterminism-linux-run{1,2}.txt`. Resolved by T042a to T042g: the re-recording on Linux after the D1 fix is byte-identical and `linux.txt` exists (proposed for approval at T043).**
- [X] T039 [US2] [RUN] Linux 5: stamp, run `ctest -LE determinism-sweep -j2 --output-on-failure` on the remote, record `baseline-images-linux.sha256`, `baseline-diffs-linux.txt` and
  `base-LastTest-linux.log`; run it a second time for `baseline-image-noise-linux.txt`. Record every Linux failure of the starting tree (the earlier specs found 7:
  #44, #45, #46, #57); they are the Linux baseline's failing set, not this spec's (CP-b on Linux allows exactly that set). **Note in validation.md §3 that this baseline is taken with the Phase 1 hook in the tree** (the hook is null in production and the macOS manifest check shows it adds no output).
- [X] T040 [US2] [RUN] Linux 5b: the same sweep manifest for Linux (B1), on the tree that holds the Phase 1 hook: stamp, `ssh "$H" "cd $RR && ~/.claude/bin/cpujob 026-l1-sweep ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure"`,
  `baseline-sweep-images-linux.sha256`, `base-sweep-LastTest-linux.log`, a second run for `baseline-sweep-noise-linux.txt`; record the sweep's failing set on the starting tree (a failure that predates this spec is not this spec's).
- [X] T041 [US6] [RUN] Linux 6: `nm -D --undefined-only build/src/libshader/shading/libshader_shading.so | c++filt > tools/libshader-imports-base-linux.txt` and
  `readelf -d build/src/libshader/shading/libshader_shading.so > tools/libshader-dynamic-base-linux.txt` (on the remote). Record whether `FLAGS` has `BIND_NOW`: it decides
  whether the Phase 2 load stub must define the undefined functions as well as the vtables (the table may differ by platform; recorded, not tagged).
- [X] T042 Linux 7: `$T/linux_sync.sh pull`, then `ls $T/*-linux* tests/shading_contract/replay/linux.txt`, and `$T/linux_sync.sh verify`. All of: `archcheck-base-linux.txt`,
  `baseline-images-linux.sha256`, `baseline-diffs-linux.txt`, `baseline-image-noise-linux.txt`, `libshader-imports-base-linux.txt`,
  `libshader-dynamic-base-linux.txt`, `base-LastTest-linux.log`, `baseline-sweep-images-linux.sha256`, `baseline-sweep-noise-linux.txt`, `base-sweep-LastTest-linux.log`, `replay/linux.txt` must exist locally. This is the gate: without them Phase 2 does not start.
  `replay/linux.txt` now exists (T042e); the final `pull` returned 11 files, 0 differences.
- [X] T042a [US6] **D1 (validation.md §4.4): find the cause** of the Linux-only non-determinism of the `.slo` sections of `transmission` and `shadow` in the replay (decided 2026-10-31: fixed inside 026, no partial `linux.txt`). No debugger. Temporary logging is allowed (an `fprintf` in `jitTraceBatch` and the JIT shadow path behind an environment variable, never staged, removed afterwards); read the strides the emitter passes (`llvmEmitter.cpp` `visibility`/`transmission`/`trace`, `rslOps.cpp` `op_*_v3`) against `shading.cpp`'s `jitTraceBatch` and `jitEnvironment`. Record the evidence (which field is garbage, on which path, and why Linux sees it and macOS does not) in validation.md §4.4 D1.
  **T042a done (2026-10-31):** cause found without a debugger: `N` is unwritten for `transmission()`, `visibility()` and `trace()` because their `DEFSHORTFUNC` flags lack `PARAMETER_N` (validation.md §4.4 D1, part 1, proved with `MallocPreScribble`); part 2: the `shadow` scene reads `L` in a surface shader (scene defect, unproved).
  **T042b/c/d (first pass): STOPPED, then option A approved:** The trial fix changes no suite image but changes bytes of the approved `macos.txt` (`transmission`, `visibility`), and the `shadow` scene would have to change too. Option A was approved and done 2026-10-31 (validation.md §4.4 D1: `PARAMETER_N` on the four builtins, `shadow` scene moved into a light shader, `macos.txt` re-recorded in three sections, `linux.txt` recorded).
- [X] T042b [US6] **Scope check.** If the cause is outside 026 (not in the trace-request setup), STOP and report before fixing anything.
- [X] T042c [US6] **Fix** with the smallest change the evidence shows (pass and honour the real strides, or initialize the field). Stage it.
- [X] T042d [US2] [RUN] **No output change on macOS**: build, the suite (CP-b) with CP-d at 0 differing and 0 missing and every `MaxBlockAvgDiff` equal to the baseline, and `ShadingContract_TraceReplay_*` green against the approved `macos.txt` **byte for byte**. If the fix changes any `.slo` image or any byte of `macos.txt`, STOP and report before going further (a changed reference needs approval).
- [X] T042e [US6] [RUN] **Linux record twice** (push, verify, build, `ctest -L shading_contract-record` twice): the two `replay-fresh.txt` must be byte-identical and have the same request and ray counts as macOS, for all 16 sections. Copy one to `tests/shading_contract/replay/linux.txt` (proposed for approval at T043; not approved before).
- [X] T042f [US6] [RUN] **Linux with the replay included**: `ctest -L '^shading_contract$'` (the 8 `TraceReplay_*` green) and the Linux suite `-LE determinism-sweep -j2` with the `TraceReplay_*` checks included (only the recorded Linux failing set may fail), CP-c, and `linux_sync.sh pull` + `verify`.
- [X] T042g Update validation.md §4.4 D1 with the cause, the fix and the evidence (the before/after of the two recordings), and §3.

- [X] T043 [US2] **Checkpoint 1 (PAUSE 1, LINUX GATE)**: stage `src/` (the hook), `tests/shading_contract/`, `tests/CMakeLists.txt`, `tools/**`, `validation.md`. Summarize: the
  macOS and Linux baselines (test counts, failing sets, noise lists), the hook's 12 call sites, the replay's per-family request counts on both platforms, the eight mutation results,
  the 6 imports, and the CP-c result. **Ask the maintainer to approve the two recorded replay files (`replay/macos.txt`, `replay/linux.txt`) as new references**; once approved they are
  never re-recorded. **Also name for approval the other new references of this phase**: the two noise lists (`baseline-image-noise-macos.txt` from `T005`, `baseline-image-noise-linux.txt` from `T039`) and the two sweep manifests with their noise lists (`T007`, `T040`). Wait for the review.
  **Approved 2026-10-31 (maintainer, via the coordinator), all recommendations:** the two noise lists, both sweep manifests with their noise lists, the suite manifests and diffs files for both platforms, `replay/macos.txt`, the three sections of it re-recorded after the D1 fix (`transmission`, `visibility`, `shadow`), `replay/linux.txt`, and the rewritten `replay_shadow.sl` / `replay_shadow.rib`; the `backends()` edit in `trace_replay.h` was ratified earlier. D4 is filed as GitHub #123 (P0, Bug Tracker). From here the replay files are never re-recorded.


---

## Phase 2: Tests first (no `src/` edit)

**Goal**: every contract test, the doubles, the load-time stub and the shaders exist, all `DISABLED`, and fail to build against the starting `src/`.
The one TEST-APPROVAL for the bulk of the test code is at the end of this phase; Phases 3 to 10 re-approve per phase only what changes.

### Tests block (everything in this phase is a test; no `src/` file changes)

- [X] T044 [US1] (**Phase 2 note, 2026-10-31: `Situations` must cover the non-default hit modes itself, T009 found none in the visual suite**) Extend `tests/shading_contract/CMakeLists.txt` under `if(OPENRENDER_SHADING_CONTRACT_TESTS)`: the executables `test_shading_contract_standalone` (links
  `libshader_shading`, `openrendercommon`, `state_only_sources` (spec 025's State-only library from `tests/unit/state/CMakeLists.txt`), `load_stub.cpp` and the doubles; **no `ribRender`**,
  nothing from `src/ri/{render,hiders,geometry}`) and `test_shading_contract_rendered` (adds `ribRender`). Both `EXCLUDE_FROM_ALL`. Each executable takes the case name as
  `argv[1]`; one ctest entry per case, every one `DISABLED TRUE`, label `shading_contract`, `TIMEOUT`, the same environment as `tests/shading_parity`. The `*Jit` entries and the `.slo`
  shader outputs exist only when `OPENRENDER_HAVE_LLVM` is on (as `tests/shading_parity/CMakeLists.txt` does). Add a comment per entry naming the phase that enables it.
  Registered cases: standalone `StandaloneLink`, `StandaloneLinkLine` (a CMake script test), `Attributes{Interp,Jit}`, `Messages{Interp,Jit}`, `Matrices{Interp,Jit}`,
  `TraceRequests{Interp,Jit}`, `NoServices_{Matrix,Attribute,Message,Tracer}`, `ShaderGlobalsDouble`; rendered `RealAnswers{Interp,Jit}_{Matrices,Attributes,Messages}`,
  `GatherRequests{Interp,Jit}`, `CannedTrace{Interp,Jit}`, `NoTracer`, `DoubleGlobalsMatchRenderer`; and `Situations` in **its own executable** `test_shading_contract_situations` (`EXCLUDE_FROM_ALL`
  until Phase 9), because it reads report fields that only Phase 9 adds and would otherwise stop the whole rendered program from compiling in Phases 3 to 8. A case that needs any other API a later phase adds gets its own
  executable the same way; test code must not name `libshader::traceRay`/`TraceKind` (Phase 8), `objectAttributes()` (Phase 7) or `lightParameter()` (Phase 6). `NoServices` is four tests because the plan's single
  `NoServices` cannot be green in Phase 3: no service is routed yet (QB, decided 2026-10-31).
- [X] T045 [P] [US1] Write `tests/shading_contract/contract_services.h`: `CContractServices`, a **standalone** `CRendererServices` v2 with no inner implementation (FR-004), recording every
  call in order (method, request fields, answer). Answers per contracts/test-doubles.md: `maxGridSize` 64; the 29 built-in globals built with `parseVariable` from the declarations
  `initDeclarations()` registers (copied as text; `DoubleGlobalsMatchRenderer` keeps the copy honest); timing, raster, clip, bounds, `additionalParameters`, `hasIlluminationHook`,
  `shootStep` from fields the test sets; `getAttribute` from a table `(scope, objectKey, name) -> (type, count, values)`, anything else `NotFound`, with a per-test switch for `Unavailable`;
  `getMessage` from a table `(source, objectKey or lightKey, name) -> default value or (variable, globalIndex)`; `getMatrix` from a table `name -> (kind, from, to)` honouring `time` when the
  test sets per-time entries and answering `NoObject` for `"object"` with a null handle; `tracer()` returns the test's `CRecordingTracer` or `nullptr`; the GI/3D getters,
  `textureSystem`, the lookups, `getDSO` and `getGlobalID` return `nullptr` or 0, recorded. Carries `static_assert(openrender::shading::kRendererServicesVersion.major == 2)`.
  **Temporary block**: the 28 methods that stay pure virtual until Phases 4 and 5 and `setOffendingObject(CObject *)` until Phase 7 are implemented in one delimited block marked
  `TEMPORARY (spec 026): deleted in phases 4, 5, 7`, each recording its call; without it the double is abstract in Phase 3 and `StandaloneLink` cannot go green (QD, decided).
- [X] T046 [P] [US6] Write `tests/shading_contract/recording_tracer.h`: `CRecordingTracer` (appends every request to a vector: kind, label, bundle depth, path depth, every ray's inputs; optional
  inner tracer; for `traceRay` it may answer canned hits, `object` = a sentinel pointer and `t`; in the standalone program it never calls the bundle protocol) and `CCannedTracer` (rendered program
  only; drives the real bundle protocol with canned answers: `postTraceAction()`, then `postShade(n, rays, varying)` with canned `Ci`/`Oi` written into
  `context.currentShadingState->varying` for "hit" rays or `postShade(n, rays)` for "miss" rays, then `post()`, repeating while `bundle->numRays > 0`, as `trace.cpp` does). Carries the version `static_assert`.
- [X] T047 [P] [US1] Write `tests/shading_contract/load_stub.cpp`: `stubTrap(const char *what)` (message to stderr, then `std::abort()`, so a test that reaches a stub fails and never hangs, FR-024) and
  the definitions that let `libshader_shading` load without `libRIBRender`: `CTraceBundle::postTraceAction()` (key function), `postShade(int, CRay **, float **)`, `postShade(int, CRay **)`, `post()`;
  the same four for `CTransmissionBundle`; `CGatherBundle::CGatherBundle()`, `~CGatherBundle()` and its four virtuals; `CPhotonMap::lookup(float *, const float *, int)`. Includes `ri/hiders/bundles.h` and
  `ri/hiders/photonMap.h` (test directory, allowed); compiled into the standalone executable only. **Plus two TEMPORARY traps, `CShadingContext::trace(CRay *)` and `CShadingContext::traceEx(CRayBundle *)`**,
  because the starting tree still imports them (6 imports, not the 4 of contracts/test-doubles.md); Phase 8 deletes them (QC, decided). Compare the table with
  `tools/libshader-imports-base-{macos,linux}.txt` and `libshader-dynamic-base-linux.txt` from Phase 1.
- [X] T048 [P] [US1] Write `tests/shading_contract/contract_harness.h`: a concrete `CShadingContext` subclass (`renderingLoop() override {}`, like `CProbeContext` in
  `tests/shading_parity/texture_substitute_harness.h`); state setup for N points with the 29 globals; loaders for `.rslo` (`parseShader`) and `.slo` (`parseSloShader` from `sloLoader.h`,
  Phase 3); a runner that executes a surface, displacement, atmosphere or light instance on N points and reads back `Ci`/`Oi`/`P`; install and restore of the services. **The standalone program
  cannot construct a `CSurface`** (that is Geometry), so its `currentObject` is null and the double is keyed on the null `ObjectHandle`; handle identity (a real `CProbeSurface`) is checked in the rendered
  program only (QE, decided). Shader outputs are produced by build steps that run `oshader` and `oshader --jit`.
- [X] T049 [P] [US3] Write the RSL shaders `tests/shading_contract/shaders/cm_attribute.sl`, `cm_option.sl`, `cm_rendererinfo.sl` (each with a float, a triple, a matrix and a string form, a
  name that exists, one that does not, and a varying name argument where RSL allows it), `cm_surface.sl`, `cm_displacement.sl`, `cm_atmosphere.sl`, `cm_incident.sl`, `cm_opposite.sl` (parameter
  queries of every type, a mutable parameter), `cm_lightsource.sl` (a light queried inside an `illuminance` loop, float and non-float).
- [X] T050 [P] [US4] Write `tests/shading_contract/shaders/cm_matrices.sl` (`transform`, `vtransform`, `ntransform`, `ptransform`, matrix forms and `texture3d`'s coordinate system, in both
  directions, among `"current"`, `"world"`, `"camera"`, a user-declared system, `"object"`, `"shader"`, `"light"` and a colour-space name) and the traced shaders `cm_trace.sl`,
  `cm_transmission.sl`, `cm_visibility.sl`, `cm_gather.sl`, `cm_occlusion.sl`, `cm_indirect.sl`, `cm_env.sl`, `cm_shadow.sl` (US6).
- [X] T051 [P] [US1] Write `tests/shading_contract/case_link.cpp` (`StandaloneLink`: installs the double, runs a trivial shader under both backends, checks the double saw its globals and nothing
  from a renderer) and `tests/shading_contract/check_link_line.cmake` (`StandaloneLinkLine`: reads the link command of `test_shading_contract_standalone` and fails if it names `libRIBRender`,
  `ribRender_static` or any object under `src/ri/{render,hiders,geometry}`; US1 AS1, SC-001).
- [X] T052 [P] [US3] Write `tests/shading_contract/case_attributes.cpp` (`Attributes{Interp,Jit}`, standalone): for `attribute()`, `option()` and `rendererinfo()` the double receives the same
  request from both backends (scope, object handle, `name[0]`, expected `QueryType` from the overload or from the JIT's `resultKind` 1/3/16/-1) and the builtin returns the double's answer; **one request
  per call, not per lane**; `NotFound` leaves the destination untouched and returns 0; a stored value smaller than the destination is copied as on master and 1 returned; the
  found/not-found result equals the documented contract. (The refusal of an unsafe `TypeMismatch` is tested in Phase 5, `T092`.) Recorded requests are compared pairwise between the two backends.
- [X] T053 [P] [US3] Write `tests/shading_contract/case_messages.cpp` (`Messages{Interp,Jit}`, standalone): for `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()` the
  requests are equal between the backends (source: `incident` -> Interior, `opposite` -> Exterior as master maps them; name; expected type; `wantVariable` true for the first three, false for the
  last two) and the answers (default value, or variable and global index for a mutable parameter, read through `jitParameterFinish`) are consumed; **`lightsource()` asserts the documented difference**
  (the interpreter asks with the iterated light, `wantVariable = true`, every type; the JIT with the last light that ran, `wantVariable = false`, float only, value written into `result`, Q2).
- [X] T054 [P] [US4] Write `tests/shading_contract/case_matrices.cpp` (`Matrices{Interp,Jit}`, standalone; US4 AS1 to AS3): every matrix comes from the double's `getMatrix` for each system kind; the
  request carries the object, the shader instance and the light instance as opaque handles and **time 0** (Q14); a double that honours time returns the per-time matrix when asked at another time; an
  `"object"` reference with a null object reports `Object system reference without an object` once and uses the identity matrix; an unknown name reports `Unknown coordinate system` and uses the
  identity; transformed values equal the values computed from the double's table.
- [X] T055 [P] [US6] Write `tests/shading_contract/case_trace_requests.cpp` (`TraceRequests{Interp,Jit}`, standalone): with a `CRecordingTracer`, `visibility()`, the probe `transmission()`
  and the reflection probe issue their rays (origin, direction, differentials, time, bias, ray type, depth, sample key, order) and consume canned single-ray answers; `trace()` and `transmission()` issue
  their **bundle requests** (recorded, never run: constructing the bundles on the stack only writes a vtable pointer). Ordering and sample identity equal the replay's.
- [X] T056 [P] [US1] Write `tests/shading_contract/case_no_services.cpp`: `NoServices_Matrix` (no services installed: `Unknown coordinate system` path, identity, one warning per kind), `NoServices_Attribute`,
  `NoServices_Message` (not found, destination untouched, one warning per kind), `NoServices_Tracer` (the probe forms: `ray->object = NULL`, one warning; the bundle forms are `NoTracer`'s).
  Each is a separate case (US1 AS3, FR-012, FR-023(f)).
- [X] T057 [P] [US5] Write `tests/shading_contract/case_globals_double.cpp` (`ShaderGlobalsDouble`): a double producer fills a `ShaderGlobals` for each situation the renderer has today (camera
  grid, camera ray hit under each hit mode, secondary hit, shadow-type hit, photon hit, a bare point) and a checker validates the documented invariants of contracts/shader-globals.md: structure of
  arrays with a runtime point count and a batch of one valid; `Strided<T>` with stride 0 and `data == nullptr` meaning absent; the derivative tail (`numDerivativeTail` 0 or `2 * numPoints`, never
  counted, never active, `sampleKey` copying the real point's); `gridU * gridV == numPoints` for a grid; inactive lanes unspecified; only `P`, `time` and the per-batch fields may be assumed.
- [X] T058 [P] [US3] Write `tests/shading_contract/case_real_answers.cpp` (`RealAnswers{Interp,Jit}_{Matrices,Attributes,Messages}`, rendered): a new complete forwarding class `CContractForwarder` in `tests/shading_contract/contract_forwarder.h` that forwards every member to the services installed before it and records the v2 ones (`CRendererServicesImpl` is `final`,
  so it cannot be derived from; the existing wrappers are not used, so this test does not depend on the wrappers, QA), real `CProbeSurface` objects (handle identity),
  real bound shaders. Each case asserts (i) every returned value equals an **oracle computed directly from `CRenderer`/`CAttributes`/the bound instances**, including found/not-found and
  the unknown-name and bad-system diagnostics, and (ii) the recorder saw the v2 call. (ii) is what makes the case red before its phase; (i) is what proves master's values are kept. **The forwarder also carries a delimited `TEMPORARY (spec 026)` block**, like `CContractServices` (QD): it forwards the methods that stay pure
  virtual until their phase (the two coordinate-system lookups, Phase 4; the 25 option getters and `lookupUserOption`, Phase 5; `setOffendingObject(CObject *)`, Phase 7). Without the block it stops compiling the moment a phase removes a member (A2).
- [X] T059 [P] [US6] Write `tests/shading_contract/case_gather_requests.cpp` (`GatherRequests{Interp,Jit}`: `gather()` constructs a `CGatherBundle`, which traps on the stub, so it is rendered),
  `case_canned_trace.cpp` (`CannedTrace{Interp,Jit}`: a `CCannedTracer` drives the real bundle protocol; `trace`, `transmission`, `gather`, `occlusion`, `indirectdiffuse` consume the canned answers),
  `case_no_tracer.cpp` (`NoTracer`: the bundle forms, `gather` and the irradiance cache with no tracer installed warn once per kind and return the value of a ray that hits nothing).
- [X] T060 [P] [US1] Write `tests/shading_contract/case_double_globals.cpp` (`DoubleGlobalsMatchRenderer`, rendered): the double's 29 globals equal the renderer's `initDeclarations()` table entry by entry.
- [X] T061 [P] [US5] Write `tests/shading_contract/case_situations.cpp` and scenes `tests/shading_contract/scenes/sit_*.rib` (`Situations`, rendered): one small scene per row 1 to 5, 8 and 9 of the situation table in
  contracts/shader-globals.md, covering every (call site, hit mode) pair `T009` found, **rows 6 and 7 excepted (classify-only, B8)**; the scenes read `dim`, `displaceOnly`, `inShadow`, `rayDepth` and `illuminationHook` from the two
  spec-039 test observers (their reports gain these as trailing fields in Phase 9); the test asserts `libshader::classifyShading()` returns the row's ray type, hit mode, transport and pass, and that the
  branch `shade()` took (primitive-only versus shaded) matches the hit mode. **Rows 6 and 7 (implicit-surface hits and DSO-object hits, row 6's present behaviour is an oddity, O5 #119; row 7 is kept classify-only as decided in B8, O2 #116 having been resolved by #129 on 2026-11-01) and rows 10 to 14 (no `shade()`) are asserted from `classifyShading()` alone** (B8). The seven remaining oddities of research R13 are asserted as
  oddities (documented, not fixed).
- [X] T062 [US1] [RUN] Red evidence by build failure, in a scratch tree: check `df -h /Volumes/Projects`, then `$CJ 026-red-configure cmake -B build-red -S . -DCMAKE_BUILD_TYPE=Release -DOPENRENDER_SHADING_CONTRACT_TESTS=ON`
  (through `cpujob`; the option is what makes the targets exist), `$CJ 026-red cmake --build build-red --target test_shading_contract_standalone test_shading_contract_rendered -j4`. The expected failure names the missing
  `serviceTypes.h`, `tracer.h`, `shaderGlobals.h` and `sloLoader.h`. Paste it into validation.md §2, then `rm -rf build-red`.
- [X] T063 [US2] [RUN] CP-a and CP-c with the option OFF (the main tree must be unaffected; no `src/` file changed since Phase 1): build, then `ctest -L architecture` and the full archcheck report
  (expect 8 / 8 / 33, `RESULT PASS`), and `ctest -N -L shading_contract` listing the new tests as disabled. CP-b is not repeated: nothing the suite runs has changed.
- [X] T064 [US1] **TEST-APPROVAL (PAUSE 2)**: stage `tests/shading_contract/`, `tests/CMakeLists.txt`, `validation.md`. Summarize: the cases and the requirement each covers (a table: test -> FR/SC/US), which
  tests are red by build failure and which are characterization tests, the double's TEMPORARY block, the stub's two TEMPORARY traps, the four-way split of `NoServices`, the rendered-only forwarding
  recorder, the red evidence, and the CP-c result. **Wait for the maintainer's approval of every new test.** The decisions QA to QG (spec.md, Clarifications, "Session 2026-10-31") are already taken. This phase's approval also ends the phase.
  **Approved 2026-10-31 (maintainer, via the coordinator): the 28 Phase 2 tests as written, deviations 2 to 9 included.** Deviation 1 (the standalone harness's `attachFakeObject()`, a zero-filled CObject-layout block, used by `TraceRequests` and `NoServices_Tracer` because the trace builtins read `currentObject->attributes`) was approved with an amendment: `objectAttributes()` stays null when there is no current object and the trace-path consumers take built-in defaults then (T129, which also serves FR-027; the maintainer chose this null-tolerant design at T066 over a default `CAttributes` set), the trace-path reads are all routed through it by Phase 7 (T130), and `attachFakeObject()` is removed in Phase 8 (T139a). A read that cannot be routed in this spec keeps the fake object for that read alone, and the report names it.


---

## Phase 3: Foundation (US1, US5; FR-001 to FR-004, FR-013, FR-015 to FR-017)

**Goal**: the new headers, the version constant, the four new service members with defaults and no caller, the `.slo` loader moved into Shading, and
the contract programs on; `libshader_shading` also configures and compiles with the JIT off (B9). Tests that go green: `StandaloneLink`, `StandaloneLinkLine`, `ShaderGlobalsDouble`, `DoubleGlobalsMatchRenderer`.
No image can change (nothing reads the new members yet).

### Tests block

- [X] T065 [US1] Test gate for Phase 3. The tests this phase turns green are `ShadingContract_StandaloneLink`, `ShadingContract_StandaloneLinkLine`, `ShadingContract_ShaderGlobalsDouble` and
  `ShadingContract_DoubleGlobalsMatchRenderer`. Their red evidence is Phase 2's build failure (`T062`), so TG step 2 is not repeated. The option `OPENRENDER_SHADING_CONTRACT_TESTS` **stays OFF at the gate**: the headers do not exist yet, so turning it on would break the build at the pause (it is turned on by `T075`). Every other contract test stays `DISABLED`. No existing test is edited in this phase.
- [X] T066 [US1] **TEST-APPROVAL (PAUSE 3a)**: list the four enabled tests and confirm no approved test is edited. A compile fix to Phase 2 test code that the real headers force (a typo, a signature) is an edit to an approved test, so it is **confirmed before it is applied**: `T075` stops and shows the maintainer the diff first;
  nothing is edited and reported afterwards. One-word reply from the maintainer to proceed with that rule.

### Implementation

- [X] T067 [US1] Create `src/libshader/shading/serviceTypes.h` exactly as contracts/renderer-services-v2.md: namespace `openrender::shading`; `ObjectHandle` and `ShaderHandle` (`const void *ptr = nullptr;
  explicit operator bool`); `EQueryStatus : uint8_t { Found, NotFound, TypeMismatch, NoObject, Unavailable }`; `QueryType : uint8_t { Float, Triple, Matrix, String }`; `AttributeScope { Attribute, Option, RendererInfo }`;
  `MessageSource { Surface, Displacement, Atmosphere, Interior, Exterior, Light }`; `AttributeRequest`; `AttributeValue` (non-copyable: copy constructor and assignment `= delete`, because `data` may point
  into its own `inlineValue[16]`; **"when `status` is `Found`, `data` is non-null and `count >= 1`"; `TypeMismatch` (FR-026) is documented as "the copy cannot be made safely: `Triple` against more than 3 floats, `Matrix` against more than 16, `String` against a non-string, any other type against a string; Shading refuses it"**; `count` is floats, or `char *` slots when `type == TYPE_STRING`); `MessageRequest`;
  `MessageAnswer` (`globalIndex = -1`); `MatrixRequest`; `MatrixAnswer` (`from`/`to` column-major 4x4, "never null when `Found`"). Includes only `common/global.h`, `common/rendererc.h`, `common/variable.h`
  (FR-003). A comment per type states validity ("valid for the duration of the shading call that made it; never stored by an implementer").
- [X] T068 [P] [US6] Create `src/libshader/shading/tracer.h`: `ITracer` with the two pure virtuals `traceRay(CShadingContext &, CRay *)` and `traceBundle(CShadingContext &, CRayBundle *)` and the contract comments
  of contracts/tracer.md verbatim (inputs, outputs, thread-safe, re-enters Shading: `traceRay` never, `traceBundle` yes). Includes `serviceTypes.h` only and forward-declares `CShadingContext`, `CRay`, `CRayBundle`.
- [X] T069 [P] [US5] Create `src/libshader/shading/shaderGlobals.h` exactly as contracts/shader-globals.md: `BatchShape`, `RayType { None, Camera, Secondary, Shadow, Photon }`, `HitMode { Shade, Primitive }`,
  `TransportMode { Radiance, Importance }`, `ShadingPass { Full, DisplaceOnly, DisplaceEstimate }` (all `uint8_t`, "append only"); `Strided<T>` (`stride 0: one value for every point`; `data == nullptr`: the field is
  absent; `present()`); `ShaderGlobals` with the per-shape, per-batch and per-point fields and the presence rules in the header comment (data-model.md "Per-point fields and presence rules": only `P`, `time` and the
  per-batch fields may be assumed; derivative tail never active; inactive lanes unspecified). `shadingCellFootprint` documented as "pixels: `CAttributes::shadingRate`, already sqrt(area), never square-rooted again".
- [X] T070 [US5] Create `src/libshader/shading/situation.h` and `situation.cpp`: `libshader::ShadingSituation`, `ShadingClass` and `classifyShading()` as a **pure function** encoding the 14-row table
  of contracts/shader-globals.md (inputs `dim`, `displaceOnly`, `inShadow`, `rayDepth`, `illuminationHook`, `rasterHider`, the three hit-mode characters). Called only by tests (FR-018); nothing in the render path
  includes it. The oddities of research R13 are encoded as the table has them (not fixed) with a comment per row naming the code location (eight when written; O2, the DLO vertex count, was then resolved by #129, so seven remain).
- [X] T071 [US1] Edit `src/libshader/shading/rendererServices.h` (depends on `T067`): add `InterfaceVersion` and `inline constexpr InterfaceVersion kRendererServicesVersion{2, 0, 0}` with the MAJOR/MINOR/PATCH
  rules and the history table (first row `2.0.0 (spec 026, <the date this task is done>)`; "version 1 was the unversioned interface of specs 024, 025, 027"); forward-declare `ITracer`; add the four members with the defaults of
  contracts/renderer-services-v2.md: `getAttribute(const AttributeRequest &, AttributeValue &answer) const` (default leaves `answer` at `Unavailable`), `getMessage(const MessageRequest &) const` (default `{}`),
  `getMatrix(const MatrixRequest &) const` (default `{}`), `ITracer *tracer()` (default `nullptr`). **Document each new member** (FR-002): inputs, outputs, the not-found result, thread-safety, whether it re-enters
  Shading, and "introduced in 2.0.0". The 57 existing methods keep their form in this phase; nothing is removed yet. The header includes no Hiders, Geometry or core header (FR-003).
- [X] T072 [US1] In `src/libshader/shading/shading.h` / `shading.cpp` add `ObjectHandle CShadingContext::objectHandle() const` (declared in the header, defined **out of line in `shading.cpp`**, which keeps `object.h`:
  `ObjectHandle{static_cast<const CObject *>(currentShadingState->currentObject)}`, null when there is no object) and a small inline helper that makes a `ShaderHandle` from a `CShaderInstance *`. Phase 4 needs them first,
  although the plan's table lists `objectHandle()` under Phase 7 (QG, decided); Phase 7 adds only `objectAttributes()` and the `setOffendingObject` change. No caller yet, so no behaviour change.
- [X] T073 [US1] Move `parseSloShader()` and its helpers (`src/ri/render/rendererFiles.cpp`, the block that references no `CRenderer::` member) **verbatim** into new
  `src/libshader/shading/sloLoader.h` and `sloLoader.cpp` (Q5). That block is the `#ifdef OPENRENDER_HAVE_LLVM` region of `rendererFiles.cpp` (from its opening `#ifdef` to the `#endif // OPENRENDER_HAVE_LLVM`), so **only the includes inside that region move with the
  function, and the guard stays** around them in `sloLoader.cpp` (and around the declaration in `sloLoader.h`): with the JIT off both files compile to nothing. `rendererFiles.cpp` keeps its own includes and the guards at its call sites (B2); `rendererFiles.cpp` includes `sloLoader.h` and keeps calling it from `getShader`. Confirm with
  `grep -n 'CRenderer::' src/libshader/shading/sloLoader.cpp` (none). No behaviour change; DB-007 is unchanged (the core still chooses the backend). Edit `src/libshader/shading/CMakeLists.txt` (+ `situation.cpp`,
  `sloLoader.cpp`) and, if `parseSloShader` was compiled into a list the move invalidates, `src/ri/CMakeLists.txt` (ribVector must still build and link).
- [X] T074 [P] [US1] In `src/ri/render/rendererServicesImpl.h` add `static_assert(openrender::shading::kRendererServicesVersion.major == 2);` (depends on `T071`). The four new members keep their defaults here. No other edit to the core.
- [X] T075 [US1] [RUN] Now turn the contract programs on: set `OPENRENDER_SHADING_CONTRACT_TESTS` to **ON by default** and remove `EXCLUDE_FROM_ALL`/`DISABLED` from the four Phase 3 tests only (every other test keeps its marker). Build the contract programs against the real headers: `$CJ 026-p3-prog cmake --build build --target test_shading_contract_standalone test_shading_contract_rendered -j4`. Fix compile errors in the
  Phase 2 test code only where the real header differs from the contract text; **stop and show the maintainer each fix's diff before applying it** (an edit to an approved test, `T066`); apply only what is confirmed, and list it at `T081`. A semantic failure of an approved test is a stop condition, not a fix.
- [X] T076 [US1] [RUN] Measure the stub: `nm -m` and `dyld_info -fixups` of `build/tests/shading_contract/test_shading_contract_standalone` and of `libshader_shading`; the standalone program must
  start (the 6 imports defined by the stub), and the table in `load_stub.cpp` must equal the measured imports exactly (a missing symbol fails the program's start; an extra one is removed). **Assert that `libshader_shading`'s imports are a subset of the baseline's**
  (`$T/libshader-imports-base-macos.txt`; SC-011: the moved `.slo` loader and the new headers add none). Record in validation.md §1 as `libshader-imports-phase3-macos.txt`.
- [X] T077 [US1] [RUN] Run the Phase 3 tests: `ctest -R 'ShadingContract_(StandaloneLink|StandaloneLinkLine|ShaderGlobalsDouble|DoubleGlobalsMatchRenderer)'` on macOS, both backends where the case has them. All green.
  `StandaloneLinkLine` proves the link command has no `libRIBRender`, `ribRender_static` or `src/ri/{render,hiders,geometry}` object (US1 AS1).
- [X] T078 [US1] [RUN] Checkpoint 3: CP-a, CP-b, **CP-c** (`ALLOWED` 8 / 8 / 33; with the contract programs on, archcheck must not exit "incomplete build tree"), **CP-d** (every image identical: nothing reads the new
  members yet), CP-m (the 8 replay tests and the 4 new tests green, the rest "Not Run (Disabled)"). Check `grep -n '#include' src/libshader/shading/{serviceTypes,tracer,shaderGlobals,situation}.h src/libshader/shading/sloLoader.{h,cpp}` shows only Shading
  and Infra headers (`sloLoader.*` includes no `renderer*.h` and nothing under `src/ri/{render,hiders,geometry}`). Record in validation.md §1.
- [X] T079 [US2] [RUN] The JIT-off compile of Shading (B9), at the checkpoint, so that the `sloLoader` move and the `OPENRENDER_HAVE_LLVM` guards are checked while they are fresh: check `df -h /Volumes/Projects`, then
  `$CJ 026-p3-nojit-configure cmake -B build-nojit -S . -DCMAKE_BUILD_TYPE=Release -DOPENRENDER_ENABLE_JIT=OFF` and `$CJ 026-p3-nojit-build cmake --build build-nojit --target libshader_shading -j4` (both through `cpujob`; the first
  configure may trigger vcpkg). It must compile clean under `-Werror` with `sloLoader.cpp` empty. Keep `build-nojit` for `T169` (Phase 10 reuses it incrementally).
- [X] T080 [US1] [RUN] CP-L for Phase 3: `$T/linux_sync.sh push` and `verify`, build at `-j2` under GCC 13 `-Werror`, the suite with a fresh stamp, CP-d against the `-linux` baseline, CP-c (8 / 8 / 33),
  CP-m. Measure the standalone program's imports with `nm -D --undefined-only` and `readelf -d` and adjust the stub's Linux table (recorded, not tagged: the stub is test code). A GCC-only error in code the maintainer has
  approved is a stop condition. Record in validation.md §3.
- [X] T081 [US1] **Checkpoint 3 (PAUSE 3)**: stage the new headers, `situation.cpp`, `sloLoader.*`, the edits to `rendererServices.h`, `shading.{h,cpp}`, `rendererServicesImpl.h`, `rendererFiles.cpp`, the CMake files and
  `validation.md`. Summarize (US1's boundary exists and is test-doubled): the headers and their documentation, the stub table on both platforms, the compile fixes to approved test code that the maintainer confirmed beforehand, and CP-a to CP-d, CP-m, CP-c,
  CP-L. Wait for review.


---

## Phase 4: `getMatrix` (US4; FR-008, FR-009)

**Goal**: every coordinate-system lookup goes through `getMatrix`; `findCoordinateSystem` and `findCoordinateSystemWithType` leave the interface.
Tests that go green: `Matrices{Interp,Jit}`, `NoServices_Matrix`, `RealAnswers{Interp,Jit}_Matrices`.

### Tests block

- [X] T082 [US4] Test gate for Phase 4 (TG steps 1 to 3). Enable `ShadingContract_Matrices{Interp,Jit}`, `ShadingContract_NoServices_Matrix` and `ShadingContract_RealAnswers{Interp,Jit}_Matrices`. [RUN] build and run
  them. Expected red: `Matrices*` fail because the double's `getMatrix` was never asked (the legacy `findCoordinateSystemWithType` of the TEMPORARY block was); `NoServices_Matrix` fails because with no services
  installed Shading takes the old path (no `Unknown coordinate system` once-warning of the new kind); `RealAnswers*_Matrices` fail on (ii), the recorder saw no `getMatrix`. Record in validation.md §2.
- [X] T083 [US4] Edit the existing wrappers (QA, decided 2026-10-31; named for approval): add a forwarding `getMatrix(const MatrixRequest &r) const override { return inner->getMatrix(r); }` to `CForwardingServices`
  (`tests/shading_parity/table_services.h`), `CRecordingServices` (`recording_services.h`, counting it with the other routed lookups only if the class already counts lookups; no new counter) and `CTextureServices`
  (`recording_texture_system.h`); `CStubServices` has no inner implementation (it answers fixed sentinels) and gets no forwarding override. Nothing else in those files changes in this task. **Planned in the implementation below and approved here**: deleting the
  `findCoordinateSystem` and `findCoordinateSystemWithType` overrides from the four classes and from the TEMPORARY blocks of `CContractServices` and `CContractForwarder` (Q3, A2), in the same edit that removes them from the interface.
- [X] T084 [US4] **TEST-APPROVAL (PAUSE 4a)**: stage the enabled tests and `T083`. Summarize the red output of `T082` and the two wrapper-edit classes (the three forwarding overrides are QA's;
  the 2 override deletions are Q3's, and the same 2 deletions from `CContractServices` and `CContractForwarder`'s TEMPORARY blocks are named here (A2)). **Wait for approval.** No `src/` edit before it.

### Implementation

- [X] T085 [US4] In `src/ri/render/rendererShadingServices.cpp` (declare it in `rendererServicesImpl.h`) implement `CRendererServicesImpl::getMatrix(const MatrixRequest &) const`: call
  `CRenderer::findCoordinateSystem()` (unchanged), then today's switch from `CShadingContext::findCoordinateSystem` (`shading.cpp`, the block that resolves object/shader/light/camera/current): object ->
  `static_cast<const CObject *>(object.ptr)->xform` (`NoObject` when the handle is null); shader and light -> `static_cast<const CShaderInstance *>(handle.ptr)->xform`, keeping today's `assert`s; camera and
  current -> `identityMatrix`; world, NDC, raster, screen, custom and the colour names as returned. Fill `kind` (the colour-space callers branch on it), `from`, `to`. The **time is ignored** (FR-008, Q14).
- [X] T086 [US4] Rewrite the body of `CShadingContext::findCoordinateSystem(name, from, to, type)` in `shading.cpp` to build a `MatrixRequest{name, objectHandle(), handle of currentShaderInstance,
  handle of currentLightInstance, 0.0f}` and call `getServices()->getMatrix()`. The diagnostics stay in Shading, **word for word and code for code**: `NoObject` -> `error(CODE_SYSTEM, "Object system reference without
  an object\n")` and the identity matrix; `NotFound`, `Unavailable` and an unknown kind -> `warning(CODE_BUG, "Unknown coordinate system: %s\n", name)` and the identity matrix. For `Unavailable` and for a
  missing services pointer, warn once per process per kind first (`std::atomic<bool>`, `CODE_NOTSTARTED`, the `serviceLookup.cpp` pattern). All 25 call sites (textual calls to `findCoordinateSystem(` or `jitFindCoordinateSystem(`: 17 in `shaderFunctions.h`, 1 `giFunctions.h`, 1
  `shaderOpcodes.h`, 3 `shading.cpp`, 3 `rslOps.cpp` through `jitFindCoordinateSystem`; the two forwarders in `shading.cpp` lead to the same member, and the count is the one `T008` took) reach this one member **while shading**, so both backends are covered with no emitter change (FR-009); confirm by `grep`. **Corrected 2026-10-31 (the #123 diagnosis):** "all 25 reach one member" does not hold at bind time. `src/ri/render/init.cpp` (the shader init/bind-time interpreter, renderer-core code, no shading context) includes `scriptOpcodes.h`/`scriptFunctions.h`, which include `shaderOpcodes.h`/`shaderFunctions.h`, and line 60 has `#define findCoordinateSystem CRenderer::findCoordinateSystem`, so the `pfrom` site (`PFROMEXPR_PRE`, `shaderOpcodes.h:513`, confirmed by the #123 diagnosis) and, by the same include chain, the other macro sites (`shaderFunctions.h` 17, `giFunctions.h` 1) expand there into a direct call of `CRenderer::findCoordinateSystem` and never reach `CShadingContext::findCoordinateSystem`. Those instantiations stay as they are: they are renderer-side, and `CRenderer::findCoordinateSystem` is the call `getMatrix` (`T085`) wraps. So the 25 is a count of textual sites, of which the ones that reach the Shading member are those expanded in Shading (the library's own interpreter and the JIT through `rslOps.cpp`/`shading.cpp`); the init.cpp expansion is a second instantiation of the same text. As part of this task, count exactly which sites expand in `init.cpp` (preprocess it with `-E` through the build's flags, `cpujob`) and record the number in validation.md; do not touch `init.cpp:60`. The `#123` bug itself (a light parameter default in a named space differing between the interpreter and the JIT) is not in this spec's code: its cause is the JIT's bind-time fallback in `rslOps.cpp` plus `init.cpp`, fixed on `fix/123-named-space-light-default` from master, so Phase 4 has no #123 task.
- [X] T086a [US4] **D8 (maintainer 2026-10-31, option A)**: lower the two-space `ctransform("from", "to", c)` (`c=SSc`) in the JIT. `llvmEmitter.cpp`: the `ctransform` branch dispatches on the proto shape (`SSc`) before the shared `cfrom`/`mfrom`/`ctransform` branch and emits `op_ctransform_ss`. `rslOps.cpp`/`rslOps.h`: `op_ctransform_ss` (next to `op_ctransform`, away from `getFromMatrix`/`getToMatrix` and the bind-time block that `fix/123-named-space-light-default` edits) mirrors `CTRANSFORMSEXPR`: both names through `getColorCoordinateSystem` (-> `jitFindCoordinateSystem`), `convertColorFrom` then `convertColorTo`. This amends FR-009 and the "no emitter change" statements for Phase 4 only. No test edit. Rebuilding `oshader` recompiles every shader: CP-d must show 0 differing images, and any change stops the phase.
- [X] T087 [US4] Remove `findCoordinateSystem` and `findCoordinateSystemWithType` from `CRendererServices` (`rendererServices.h`), from `CRendererServicesImpl` (`.h` and `.cpp`), and **their overrides from
  `CForwardingServices`, `CRecordingServices`, `CStubServices`, `CTextureServices` and the TEMPORARY blocks of `CContractServices` and `CContractForwarder`** (Q3, A2, approved in `T084`). `findCoordinateSystem` (3 arguments) has no caller today and
  `findCoordinateSystemWithType` had one. Verify `grep -rnE '(->|\.)findCoordinateSystemWithType\(' src tests` is empty and `grep -rn 'findCoordinateSystem(' src | grep -v 'CShadingContext::'` shows no services call (the direct `CRenderer::findCoordinateSystem` calls through `init.cpp:60`'s `#define`, and `CRenderer`'s own callers, remain and are expected).
- [X] T088 [US4] [RUN] Build and run the Phase 4 tests: `Matrices{Interp,Jit}`, `NoServices_Matrix`, `RealAnswers{Interp,Jit}_Matrices` all green; the transformed values equal the oracle.
- [X] T089 [US4] [RUN] Checkpoint 4: CP-a, CP-b, CP-c (8 / 8 / 33), **CP-d**, CP-m. Also `ctest -L shading_parity` (the four wrappers' users: `ShadingParity_*`, `ShadingServices_*`, `TextureSubstitute*`) green with only the
  approved edits. Record in validation.md §1.
- [X] T090 [US4] [RUN] CP-L for Phase 4 (push, verify, build, suite, CP-d against `-linux`, CP-c, CP-m).
- [X] T091 [US4] **Checkpoint 4 (PAUSE 4)**: stage; summarize (US4 done: every coordinate-system lookup goes through `getMatrix`, 2 methods removed, 26 to go); CP results; wait for review.


---

## Phase 5: `getAttribute` (US3; FR-005, FR-007)

**Goal**: `attribute()`, `option()` and `rendererinfo()` ask `getAttribute`; the name tables live in the renderer; the 26 option members leave the
interface. Tests that go green: `Attributes{Interp,Jit}`, `NoServices_Attribute`, `RealAnswers{Interp,Jit}_Attributes`.

### Tests block

- [X] T092 [P] [US3] Write `tests/shading_contract/case_type_mismatch_attribute.cpp` (`ShadingContract_TypeMismatch_Attribute{Interp,Jit}`, standalone; FR-026, SC-013, US3 AS4): the double answers `TypeMismatch` (with a
  stored string asked for as a float, a stored matrix asked for as a vector, a stored number asked for as a string) and the test puts a **canary** (a guard pattern on both sides of the destination) around the
  destination: the builtin returns 0, the canary and the destination are untouched, and exactly one warning of the kind is issued however many times it is called. A `Found` answer smaller than the destination is
  copied as on master (a partial write) and returns 1. The same assertions for `option()` and `rendererinfo()`, under both backends.
- [X] T093 [P] [US3] Write `tests/shading_contract/case_type_mismatch_real.cpp` (`ShadingContract_TypeMismatchReal_Attribute{Interp,Jit}`, rendered, **in an executable of its own**, `test_shading_contract_typemismatch_attr`, `EXCLUDE_FROM_ALL`: it calls `attributes()`/`options()`/`rendererInfo()` with the new `QueryType` argument, so it cannot compile before this phase, B10) and `tests/shading_contract/case_null_object.cpp`
  (`ShadingContract_NullObject_Attributes{Interp,Jit}`, rendered; FR-027, SC-014, US3 AS5). The first calls `CShadingContext::attributes()`/`options()` **directly with a canary-padded buffer** against the real
  services: `user:` attributes holding a matrix asked for as a vector and a string asked for as a float are refused with the canary intact, while `option("Format", fmt[3])` (the `scene_state_probe.sl` form) is
  still copied and returns 1; **a real `rendererinfo()` mismatch** is covered too (a numeric `rendererinfo` name asked for as a string, and the string name asked for as a float: refused, canary intact; `rendererinfo("renderer", s)` with a string: copied). The second runs a shader through the real services with `currentObject = nullptr` (as the imager does): `attribute()` answers 0 with no crash, and `option()` and `rendererinfo()`
  still answer.
- [X] T094 [P] [US3] Write `tests/shading_contract/check_message_accessors.cmake` (`ShadingContract_MessageAccessors`, a CMake script test; FR-029): it reads `src/libshader/shading/shaderFunctions.h` and fails unless
  every `DEFFUNC(Incident*, "incident", ...)` line passes `ACCESSOR_INTERIOR` and every `DEFFUNC(Opposite*, "opposite", ...)` line passes `ACCESSOR_EXTERIOR` (the source Shading asks for, `Interior` and `Exterior`), **and
  unless `CShadingContext::jitIncidentParameter` and `jitOppositeParameter` in `shading.cpp` (the `jitParameterFinish` calls at about `:3846` and `:3854`) pass the same two constants** (B3).
  Red today: the lines and the two JIT members pass them the other way round. No output depends on it.
- [X] T095 [US3] Test gate for Phase 5 (TG steps 1 to 3). Enable `ShadingContract_Attributes{Interp,Jit}`, `ShadingContract_NoServices_Attribute`, `ShadingContract_RealAnswers{Interp,Jit}_Attributes` and the new `ShadingContract_TypeMismatch_Attribute{Interp,Jit}`,
  `ShadingContract_TypeMismatchReal_Attribute{Interp,Jit}` (remove `EXCLUDE_FROM_ALL` from `test_shading_contract_typemismatch_attr`), `ShadingContract_NullObject_Attributes{Interp,Jit}` and `ShadingContract_MessageAccessors`. [RUN] build and run
  them. Expected red for the new ones: the canary is overwritten (`TypeMismatch_Attribute*`); `TypeMismatchReal_Attribute*` fails to **build** (the members have the old signature; that is its red evidence); a crash on the null object (`NullObject_Attributes`; a crash is red evidence); the crossed accessors (`MessageAccessors`). Expected red for the others: `Attributes*` fail because the double's `getAttribute` was never asked (and the standalone program may crash on the null object that `attribute()` dereferences on master: a crash is
  red evidence, record it as such); `NoServices_Attribute` fails because `options()` without services returns 0 silently (no warning of the new kind); `RealAnswers*_Attributes` fail on (ii). Record in validation.md §2. **Deviation (T093/T095, 2026-10-31):** `test_shading_contract_typemismatch_attr` stays `EXCLUDE_FROM_ALL` until T105 (its sources cannot compile before the implementation, so removing the marker now would break the default build for the whole red period); its red evidence is the failed build of that target (15 errors, all `too many arguments to function call, expected 2, have 3`), and the two `TypeMismatchReal` tests are registered ENABLED, so they report "Not Run" until the target is built.
- [X] T096 [US3] Edit the existing wrappers (QA, decided 2026-10-31; named for approval): add a forwarding `getAttribute(const AttributeRequest &r, AttributeValue &a) const override { inner->getAttribute(r, a); }` to
  `CForwardingServices`, `CRecordingServices` and `CTextureServices` (not `CStubServices`: it has no inner implementation). **Planned in the implementation below and approved here**: deleting the overrides of the 26 option members
  (`xres`, `yres`, `frame`, `frameAR`, `cropLeft`, `cropTop`, `cropRight`, `cropBottom`, `fstop`, `focallength`, `focaldistance`, `bucketWidth`, `bucketHeight`, `colorQuantizer`, `depthQuantizer`,
  `pixelFilterWidth`, `pixelFilterHeight`, `gamma`, `gain`, `maxRayDepth`, `relativeDetail`, `pixelXsamples`, `pixelYsamples`, `samplingSampler`, `samplingSeed` = 25, and `lookupUserOption`) from the four
  classes and from the TEMPORARY blocks of `CContractServices` and `CContractForwarder` (Q3, A2). A test that reads one of those getters through a wrapper must be named at the approval (expected: none; `grep` first).
- [X] T097 [US3] **TEST-APPROVAL (PAUSE 5a)**: stage the new and enabled tests and `T096`; summarize the red output, the new tests for F1, F2 and F4 and what each pins, and the edits (3 forwarding overrides, QA; 26 override deletions from the four classes, Q3, and the same deletions from the two TEMPORARY blocks, A2). **Wait for approval.**

### Implementation

- [X] T098 [US3] In `src/ri/render/rendererShadingServices.cpp` (declare in `rendererServicesImpl.h`) implement `CRendererServicesImpl::getAttribute` for scope `Attribute`: move the name table of
  `CShadingContext::attributes()` (`shading.cpp`) **verbatim**, same names, same casts (for example `(float)currentAttributes->shadingRate`), same order of checks, `user:` attributes, the computed
  answers (`Sides` from the flags and the like) written into `answer.inlineValue`, stored values returned by pointer into the renderer's own storage (`attributes->name`, `var->defaultValue`). Read the
  attributes of `static_cast<const CObject *>(object.ptr)`; **a null object answers `NotFound`** (Q12). Set `type`, `count` (floats, or `char *` slots) and `status`: `Found`, or **`TypeMismatch` per FR-026**: `expected` is `Triple` and the stored value has more than 3 floats, `Matrix` and more than 16, `String` and the stored value is not a
  string, or any other type and the stored value is a string. A smaller stored value and every float-class query are `Found` (the builtin does not know whether the destination is a scalar or an array). Do not change a
  single expression of the name table itself.
- [X] T099 [US3] Same file, scope `Option`: move `CShadingContext::options()`'s table verbatim, reading the `CRenderer` statics directly where it called the option getters (`(float)svc->xres()` becomes
  `(float)CRenderer::xres`, and so on for the 25 getters) and `lookupUserOption`'s logic for `user:` options. The object is ignored. Keep every cast and every default.
- [X] T100 [US3] Same file, scope `RendererInfo`: move `CShadingContext::rendererInfo()`'s constants (`OPENRENDER_PROJECT_NAME`, `VERSION_*`) verbatim.
- [X] T101 [US3] Add the expected type to the interpreter's macro: change `PARAMETEREXPR_PRE(accessor)` in `src/libshader/shading/shaderFunctions.h` to take a second argument `expected`, and update the 32 `DEFFUNC` lines
  that use it (F -> `Float`, V -> `Triple`, S -> `String`, M -> `Matrix`; locate by `grep -n 'PARAMETEREXPR_PRE' shaderFunctions.h`). Keep the macro's source selection (`locals[accessor][var->entry]` /
  `varying[...]`) and the varying-to-uniform guard exactly. The same argument serves Phase 6's messages. **In the same edit (F4, FR-029)** make the accessor constants of the eight `incident`/`opposite` lines agree with
  the source: `ACCESSOR_INTERIOR` for `incident`, `ACCESSOR_EXTERIOR` for `opposite`, **and make `jitIncidentParameter` and `jitOppositeParameter` (`shading.cpp`, about `:3846` and `:3854`) pass the same constants to `jitParameterFinish`** (never read for these two, so no output changes);
  `ShadingContract_MessageAccessors` turns green. **`PARAMETEREXPR_PRE` also serves `surfaceParameter`, `displacementParameter`, `atmosphereParameter`, `incidentParameter` and `oppositeParameter`** (A3): the macro now
  passes `expected` to whatever `FUNCTION` names, so in this same edit those five members and their `jit*Parameter` entry points accept a `QueryType expected` argument, **ignored until Phase 6** (the `jit*` ones pass `QueryType::Float`
  as a placeholder; `T116` replaces it with the `resultKind` mapping). Note for the coordinator: item 3 of #120 (the crossed accessors) goes stale when this task lands.
- [X] T102 [US3] Rewrite the bodies of `CShadingContext::attributes()`, `options()` and `rendererInfo()` in `shading.cpp` to take a `QueryType` argument, build an `AttributeRequest` (one request per call, made
  with `name[0]` before the per-lane copy, scope, `objectHandle()`, `expected`) and ask `getServices()->getAttribute(request, answer)` with a caller-owned `AttributeValue`. `Found` copies `count` floats (or `char *` slots) from `answer.data` into `dest` in the stored layout and returns 1. **`TypeMismatch` is refused (F1, FR-026; supersedes Q4)**: `dest` untouched, return 0, and one warning
  per builtin kind per process with a code that is not swallowed (memory: `CODE_LOG` is; use `CODE_BADTOKEN`). `NotFound` and `Unavailable` leave `dest`
  untouched and return 0 (F2: with a null object the service answers `NotFound`, and no `currentObject->` dereference remains in these three members); `Unavailable` and missing services warn once per process per kind first (`CODE_NOTSTARTED`). `options()` without services used to return 0 silently; it now warns once. **Public wrappers (decided with the tests of T093, for approval at T097):** `attributes()`, `options()` and `rendererInfo()` are private, so the real-services mismatch test and the existing `queryOption()` callers go through the public wrappers: `queryAttribute(void *dest, const char *name, QueryType expected)` (and `queryOption`, `queryRendererInfo`) take the expected type, and the two-argument forms stay as overloads for the callers that exist (`rslBuiltins.cpp`'s `float *out`, `tests/unit/rib_parse/test_sampling_option_rsl.cpp`'s `float *` and `const char **`): `(float *dest, name)` asks `Float`, `(const char **dest, name)` asks `String`. No existing caller changes. **Warning text:** `CODE_BADTOKEN`, severity warning, one per builtin kind per process, and the message names the builtin (`attribute`, `option` or `rendererinfo`, and none of the other two names), which `case_type_mismatch_*.cpp` match on.
- [X] T103 [US3] In the JIT's `jit*Parameter` entry points that call `attributes()`/`options()`/`rendererInfo()` (`shading.cpp`, the `jit` parameter functions), map `resultKind` to the expected type
  (1 -> `Float`, 3 -> `Triple`, 16 -> `Matrix`, -1 -> `String`) with one shared helper `queryTypeOfResultKind(int)` in `shading.cpp`, which Phase 6 reuses. No `op_*` signature and no emitter change; the interpreter and the JIT now issue identical requests.
- [X] T104 [US3] Remove the 25 option getters and `lookupUserOption` from `CRendererServices`, `CRendererServicesImpl` (`.h` and `.cpp`), the four test classes and the TEMPORARY blocks of `CContractServices` and `CContractForwarder` (Q3, A2, approved in
  `T097`). Verify `grep -rnE '(->|\.)(xres|yres|frame|lookupUserOption|pixelXsamples|pixelYsamples|samplingSeed|samplingSampler)\(' src tests` shows no services call left (the CRenderer statics keep their
  names; look at the matches, not the count).
- [X] T105a [US3] **D9 (maintainer 2026-11-01)**: in `llvmEmitter.cpp`, the branch for `surface`/`displacement`/`atmosphere`/`incident`/`opposite`/`attribute`/`option`/`rendererinfo` maps the proto letters `C`, `N` and `P` to `resultKind` 3 together with `V`, so a color, normal or point destination is a triple under the JIT as it is in the interpreter (`DEFLINKFUNC` -> `f=SV`). Amends FR-009 for Phase 5 only. Also approved with it (T1, T2): the sink of `case_type_mismatch_real.cpp` and `case_null_object.cpp` is installed after `RiBegin`, and `case_type_mismatch_real.cpp` inserts its user attributes through a `noinline` helper (GCC 13 `-Warray-bounds`, the family of D2). No issue is filed.
- [X] T105 [US3] [RUN] Build and run the Phase 5 tests: `Attributes{Interp,Jit}`, `NoServices_Attribute`, `RealAnswers{Interp,Jit}_Attributes`, `TypeMismatch_Attribute*`, `TypeMismatchReal_Attribute*`, `NullObject_Attributes*` and `MessageAccessors` all green. The real renderer's every value, the found/not-found
  results and the diagnostics equal the oracle (US3 AS2).
- [X] T106 [US3] [RUN] Checkpoint 5: CP-a, CP-b, CP-c (8 / 8 / 33), **CP-d**, CP-m, and `ctest -L shading_parity` (the `.rslo`/`.slo` parameter-equivalence tests). Record in validation.md §1.
- [X] T107 [US3] [RUN] CP-L for Phase 5.
- [X] T108 [US3] **Checkpoint 5 (PAUSE 5)**: stage; summarize (`attribute()`, `option()`, `rendererinfo()` through `getAttribute`; 26 more methods removed, 28 of 28 done; F1 for the attribute family, F2 for the attribute family and F4 closed); CP results; wait for review.


---

## Phase 6: `getMessage` (US3; FR-006, FR-007)

**Goal**: `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()` and `lightsource()` ask `getMessage`; `lightsource()` keeps each
backend's own behaviour (Q2). Tests that go green: `Messages{Interp,Jit}`, `NoServices_Message`, `RealAnswers{Interp,Jit}_Messages`.

### Tests block

- [X] T109 [P] [US3] Write `tests/shading_contract/case_type_mismatch_message.cpp` (`ShadingContract_TypeMismatch_Message{Interp,Jit}`, standalone; FR-026, SC-013, US3 AS4) and
  `case_lightsource_no_loop.cpp` (`ShadingContract_LightsourceNoLoop{Interp,Jit}`, standalone; FR-028, SC-014, US3 AS6). The first: the double answers `TypeMismatch` for `surface()`, `displacement()`,
  `atmosphere()`, `incident()` and `opposite()` (a string parameter asked for as a float, a matrix asked for as a vector); a canary beside the destination stays intact, the builtin returns 0 and one warning
  per kind is issued; and **the double records that it was asked to decide before any write** (its `dest` is canary-checked at the moment of the call). The second: `lightsource()` called in a surface
  outside any `illuminance`/`illuminate` loop answers not found and the process survives, under both backends (the interpreter dereferences a null light on master; the JIT already guards it, which this pins).
- [X] T110 [P] [US3] Write `tests/shading_contract/case_type_mismatch_real_message.cpp` (`ShadingContract_TypeMismatchReal_Message{Interp,Jit}`, rendered, **in an executable of its own**, `test_shading_contract_typemismatch_msg`, `EXCLUDE_FROM_ALL`: it calls the five parameter members, which gain the `QueryType` argument in Phase 5 (`T101`), so it compiles from Phase 5 on; it keeps an executable of its own and stays `EXCLUDE_FROM_ALL` until this phase's gate, N1), `tests/shading_contract/case_lightsource_no_loop_real.cpp` (`ShadingContract_LightsourceNoLoopReal{Interp,Jit}`, rendered, real services: `lightsource()` outside any `illuminance`/`illuminate` loop answers not found and does not crash; SC-014 asks for it against the real renderer, A4) and `tests/shading_contract/case_null_object_messages.cpp` (`ShadingContract_NullObject_Messages{Interp,Jit}`, rendered; FR-027, SC-014, US3 AS5; `T093`'s file is not edited). The first calls the five
  parameter members directly against a real bound surface with a canary-padded buffer (a `string` parameter asked for as a float, a `matrix` parameter asked for as a vector: refused, canary intact; a `float`
  parameter asked for as a float: copied). The second runs a shader through the real services with `currentObject = nullptr`: `surface()`, `displacement()`, `atmosphere()`, `incident()`, `opposite()` answer 0 and
  `shadername("surface")` returns the empty string, with no crash.
- [X] T111 [US3] Test gate for Phase 6 (TG steps 1 to 3). Enable `ShadingContract_Messages{Interp,Jit}`, `ShadingContract_NoServices_Message`, `ShadingContract_RealAnswers{Interp,Jit}_Messages` and the new `ShadingContract_TypeMismatch_Message{Interp,Jit}`,
  `ShadingContract_TypeMismatchReal_Message{Interp,Jit}` (remove `EXCLUDE_FROM_ALL` from `test_shading_contract_typemismatch_msg`), `ShadingContract_NullObject_Messages{Interp,Jit}`, `ShadingContract_LightsourceNoLoop{Interp,Jit}` and
  `ShadingContract_LightsourceNoLoopReal{Interp,Jit}`. [RUN] build and run
  them. Expected red for the new ones: the canary is overwritten (`TypeMismatch_Message*`); `TypeMismatchReal_Message*` builds and fails at run time: its red evidence is the **overwritten canary**, because the five members ignore `expected` until `T116`; a crash on the null object or the null light
  (`NullObject_Messages`, `LightsourceNoLoop` and `LightsourceNoLoopReal` under the interpreter; a crash is red evidence). Expected red for the others: `Messages*` fail because the double's `getMessage` was never asked (the standalone program may crash on the null object; record a crash as red evidence); `NoServices_Message` fails because no
  warning of the new kind is issued; `RealAnswers*_Messages` fail on (ii). Record in validation.md §2.
- [X] T112 [US3] Edit the existing wrappers (QA, decided 2026-10-31; named for approval): add a forwarding `getMessage(const MessageRequest &r) const override { return inner->getMessage(r); }` to
  `CForwardingServices`, `CRecordingServices` and `CTextureServices` (not `CStubServices`: it has no inner implementation). No deletion in this phase.
- [X] T113 [US3] **TEST-APPROVAL (PAUSE 6a)**: stage the new and enabled tests and `T112`; summarize the red output, the asserted `lightsource()` difference (Q2), the new tests for F1, F2 and F3 and what each pins, and the forwarding override. **Wait for approval.**

### Implementation

- [X] T114 [US3] In `src/ri/render/rendererShadingServices.cpp` implement `CRendererServicesImpl::getMessage(const MessageRequest &) const`. For the five object sources resolve the bound instance as
  `shaderInstanceOf(attributes->surface())` and the like (`Surface`, `Displacement`, `Atmosphere`, `Interior`, `Exterior`; `libshader/shading/shaderRefs.h`) from
  `static_cast<const CObject *>(object.ptr)->attributes`; a null object answers `NotFound` (Q12). For `Light` use `static_cast<CShaderInstance *>(light.ptr)`. Then call the instance's
  `getParameter(name, dest, wantVariable ? &var : nullptr, wantVariable ? &globalIndex : nullptr)` exactly as `surfaceParameter()` and its siblings do today, and return `MessageAnswer{status, var, globalIndex}`.
- [X] T115 [US3] Add a **non-writing** lookup of a parameter's shape to the shader instance: a virtual on `CShaderInstance` returning the stored `EVariableType` and `numItems` (floats for a
  vector-class or matrix parameter), with a default of "unknown, no check" so the hard-coded lights (`CSphereLight`, `CQuadLight`) are unaffected, overridden by `CProgrammableShaderInstance` with the same loop over
  `parameters` that `getParameter` (`shader.cpp`) uses, writing nothing. `getParameter` itself is not changed. In `CRendererServicesImpl::getMessage` call it first and answer `TypeMismatch` per FR-026 (`Triple`
  against more than 3 floats, `Matrix` against more than 16, `String` against a non-string, any other type against a string) **before** `getParameter` writes `dest`. **Not for source `Light`** (B4): the lookup is skipped
  there, so `lightsource()` keeps each backend's output (Q2, FR-022); the five object sources are checked.
- [X] T116 [US3] Rewrite `CShadingContext::surfaceParameter`, `displacementParameter`, `atmosphereParameter`, `incidentParameter` and `oppositeParameter` (`shading.cpp`) to build a `MessageRequest` (one request
  per call, `name[0]`, source per master's mapping **including `incident` -> Interior and `opposite` -> Exterior**, `wantVariable` true for the first three and false for the last two, expected type) and ask
  `getServices()->getMessage()`. **Also (A3)** replace the placeholder that `T101` left in `jitSurfaceParameter`, `jitDisplacementParameter`, `jitAtmosphereParameter`, `jitIncidentParameter` and `jitOppositeParameter`: they map `resultKind` to the
  expected type (1 -> `Float`, 3 -> `Triple`, 16 -> `Matrix`, -1 -> `String`) with the helper `T103` introduced, and pass it to the member. `PARAMETEREXPR_PRE`'s source selection and the varying-to-uniform guard, and `jitParameterFinish`, stay as they are; `incident()`/`opposite()` skip mutable parameters as today. A
  missing services pointer or `Unavailable` warns once per process per kind and returns not found. **`TypeMismatch` is refused (F1)**: return 0 without touching `dest`, and warn once per builtin kind per process
  (`CODE_BADTOKEN`, not the swallowed `CODE_LOG`). No `currentObject->` dereference remains in these five members: a null object reaches the service as a null handle and answers `NotFound` (F2).
- [X] T117 [US3] Add `int CShadingContext::lightParameter(void *dest, const char *name, CVariable **var, int *globalIndex)` (declared in `shading.h`, defined in `shading.cpp`) and use it in
  `LIGHTPARAMETEREXPR_PRE` (`shaderFunctions.h`): it asks `getMessage` with source `Light`, the light `currentShadingState->currentLight->instance` (the one being iterated) and `wantVariable = true`, and keeps the macro's
  `savedState[2 + globalIndex]` read, every type (F, V, S, M) and the found flag. **No `TypeMismatch` check for source `Light`** (B4): the service does not apply it, and `lightParameter()` copies as before. **Outside an `illuminance` loop `lightParameter()` returns 0 for a null `currentLight` and the macro leaves `dest` untouched (F3, FR-028)**; the macro tests `*currentLight` before it calls. Inside a loop nothing changes.
- [X] T118 [US3] Change only the **body** of `op_lightsource_f` (`src/libshader/shading/rslOps.cpp`) to ask `getMessage` with source `Light`, the light `currentLightInstance` (the last light that ran),
  `wantVariable = false`, expected `Float`, and keep writing the value into `result`, floats only, mutable values ignored; **no `TypeMismatch` check for source `Light`** (B4). No emitter change, no `op_*` signature change, no ABI change. Each backend's output is
  unchanged (FR-022); the divergence (Q2: which light, mutable values, types, what `result` holds) is out of scope: it is filed as #115 (evidence is added as a comment on it), and it is named in DB-018's "still open" in Phase 10.
- [X] T119 [US3] Null-guard `CShadingContext::shaderName(const char *type)` (`shading.cpp`): with no current object it returns `""`, the value it already returns when the shader of that type is not bound (F2, FR-027, Q8's direct
  read stays). No other change to the function. `shadername()` with no argument is unchanged.
- [X] T120 [US3] [RUN] Build and run the Phase 6 tests: `Messages{Interp,Jit}` (seven builtins' requests equal between backends; `lightsource()` asserts the documented difference),
  `NoServices_Message`, `RealAnswers{Interp,Jit}_Messages`, `TypeMismatch_Message*`, `TypeMismatchReal_Message*`, `NullObject_Messages*`, `LightsourceNoLoop*` and `LightsourceNoLoopReal*` all green.
- [X] T121 [US3] [RUN] Checkpoint 6: CP-a, CP-b, CP-c (8 / 8 / 33), **CP-d**, CP-m, and `ctest -L shading_parity` (including the `.slo` message-passing parity tests). Record in validation.md §1.
- [X] T122 [US3] [RUN] CP-L for Phase 6.
- [X] T123 [US3] **Checkpoint 6 (PAUSE 6)**: stage; summarize (US3 done: the eight builtins ask the services; F1, F2 and F3 closed; `lightsource()`'s divergence is #115, out of scope); CP results; wait for review. **This is the MVP boundary** (see the strategy).


---

## Phase 7: DB-006, the executor's reads and the offending object (US3, US7; FR-003, FR-019)

**Goal**: `execute.cpp` and `executeMisc.cpp` stop including `surface.h`; `setOffendingObject` takes an `ObjectHandle`. Red: the allowlist deletion
(2 DB-006 lines). Green: `ctest -L architecture` with exactly 2 fewer lines.

### Tests block

- [X] T124 [US7] Write `tests/shading_contract/case_offending_object.cpp` (`ShadingContract_OffendingObject`, rendered, interpreter only, **in an executable of its own**, `test_shading_contract_offending`, `EXCLUDE_FROM_ALL`: it asserts the
  `setOffendingObject(ObjectHandle)` signature, which does not exist before this phase, C6/B10; FR-003, Q11). It runs a shader that reaches `scripterror` (`execute.cpp`: the macro at about `:181`; the only trigger is the
  `"Invalid uniform lighting call"` use at about `:691`; read the code around it for a construct that reaches it) on a real `CProbeSurface` with the `CContractForwarder` installed, and asserts that the forwarder recorded
  `setOffendingObject` with an `ObjectHandle` equal to the surface's pointer, once, and that the error text is unchanged. **If no RSL construct reaches `scripterror`, do not stall the Test gate (N6)**: fall back, in the same
  executable, to a direct call of the `rendererSetOffendingObject(ObjectHandle)` helper on a context whose `currentObject` is the `CProbeSurface`, and assert the same recording; say in validation.md §2 which form was used. Remove `EXCLUDE_FROM_ALL` from the executable in `T125`.
- [X] T125 [US7] Test gate for Phase 7 (the red is the registry check). Delete exactly these two lines from `tests/architecture/allowlist.txt`, **by key**: `DB-006  include  src/libshader/shading/execute.cpp  ->
  src/ri/geometry/surface.h` and `DB-006  include  src/libshader/shading/executeMisc.cpp  ->  src/ri/geometry/surface.h`. Do not touch `shading.cpp -> object.h`. Change nothing else in the file yet (the header counts are Phase 10's). Also enable `ShadingContract_OffendingObject` (remove `EXCLUDE_FROM_ALL` from `test_shading_contract_offending`).
- [X] T126 [US7] [RUN] `$CJ 026-p7-red ctest --test-dir build -L architecture --output-on-failure`: it must report **exactly two `VIOLATION` lines, those two edges**, and nothing else (no `STALE`, no other
  `VIOLATION`). Also build `test_shading_contract_offending`: it must fail to build (no `setOffendingObject(ObjectHandle)` yet), which is that test's red evidence. Record both in validation.md §2. Any other architecture line means the design is wrong: stop.
- [X] T127 [US3] Plan the double edits (approved here, applied with the interface change below): `setOffendingObject(CObject *)` becomes `setOffendingObject(ObjectHandle)` in `CForwardingServices`,
  `CRecordingServices`, `CStubServices` and `CTextureServices`, and the TEMPORARY blocks of `CContractServices` and `CContractForwarder` are deleted (they hold only that method by now) and replaced by the real recording override. No forwarding
  override is added to a wrapper in this phase. Check by `grep` that no existing test calls `setOffendingObject` with a `CObject *` other than through these classes.
- [X] T128 [US7] **TEST-APPROVAL (PAUSE 7a)**: stage the allowlist deletion and the planned edits; summarize `T126` (the two `VIOLATION`s and the `OffendingObject` build failure) and the double edits (Q3's `setOffendingObject` change in the four classes, plus the deletion of the last TEMPORARY entries of `CContractServices` and
  `CContractForwarder`, A2). The one new test is `T124`. **Wait for approval.**

### Implementation

- [X] T129 [US7] Add `const CAttributes *CShadingContext::objectAttributes() const` (declared in `shading.h`, defined out of line in `shading.cpp`, which keeps `object.h`: it returns
  `currentShadingState->currentObject->attributes`, and **null when there is no current object**). It exists so that `execute.cpp` and `executeMisc.cpp` stop needing `CSurface`'s complete type.
  **Amended at T064/T066 (2026-10-31, null-tolerant design decided by the maintainer):** its consumers on the trace path take built-in defaults when it is null, so the standalone program's trace cases run with no object and the executor
  no longer dereferences a null object (FR-027). The consumers are the parameter-list lookups' `init` (`plBegin` and `jitParameterList`: `init` always runs; **corrected 2026-11-01 (maintainer, found in Phase 8)**: the scratch pad holds zeros, not defaults, so skipping `init` produced no rays. With a null attribute set `init` still sets samples 1, sample base 1, maximum distance infinity and an empty label, and the bias takes the attribute default 0.01 (the value `CAttributes`' constructor gives) through `traceBias()` in `shaderPl.cpp`; no `CAttributes` is constructed in Shading) and the two flag reads (`flags & ATTRIBUTES_FLAGS_SAMPLEMOTION` in `traceTransmission`, and `interior()`, `exterior()`, `flags` in `traceReflection`: no motion sampling, no interior or exterior shader).
  **No `static CAttributes` goes into `libshader_shading`** (a constructed default would add the imports `CAttributes::CAttributes()` and its vtable, which SC-011 forbids). Keep the import measurement at the Phase 7 checkpoint
  (`nm -m` / `dyld_info` and the Linux `nm -D`) as a check: expect no growth.
- [X] T130 [US7] Route the **seven** reads that are compiled into `execute.cpp`/`executeMisc.cpp`/`shaderFunctions.h`/`giFunctions.h` and read the object's attributes through `objectAttributes()` (research R5; the eighth site, `scripterror`, passes the object pointer and goes through `objectHandle()` in `T131`, "7 + 1"): `execute.cpp` `plBegin` (`lookup->init(scratch, attributes)`) and
  `runAmbientLights` (`lightSources`); `executeMisc.cpp` `flags & ATTRIBUTES_FLAGS_SAMPLEMOTION` (transmission) and `interior()`, `exterior()`, `flags` in `trace()`; `shaderFunctions.h`
  `flags & ATTRIBUTES_FLAGS_INSIDE` (`calculatenormal`); `giFunctions.h` `photonEstimator` (two sites). Locate by `grep -rn 'currentObject->attributes' src/libshader`. **The reads inside `shading.cpp` stay direct**
  (that unit keeps `object.h`; `shade()` takes the primitive until spec 029). A read that proves to need more than `CAttributes` is a stop condition.
  **Amended at T064/T066:** the trace path is read through `objectAttributes()` end to end, so these reads go through it as well, as the one exception to "the reads inside `shading.cpp` stay direct": `jitParameterList`'s `lookup->init(scratch, currentShadingState->currentObject->attributes)` (`shading.cpp`, the JIT twin of `plBegin`). Together with `plBegin`, `executeMisc.cpp:720` and `:846` that is every attribute read that `visibility()`, `transmission()` and `trace()` (probe and bundle, both backends) make. Each of the four takes the built-in default when `objectAttributes()` is null (T129). Any other read on that path that cannot be made null-tolerant is named in the Phase 7 report (it keeps `attachFakeObject()` for itself, T139a).
- [X] T131 [US7] Change `setOffendingObject` to take an `ObjectHandle`: in `rendererServices.h` (`virtual void setOffendingObject(openrender::shading::ObjectHandle object) = 0;`), the shading context's
  `rendererSetOffendingObject(ObjectHandle)` helper (`shading.h`/`shading.cpp`), `execute.cpp`'s `scripterror` (now `rendererSetOffendingObject(objectHandle())`, which removes the upcast that needed `surface.h`),
  `CRendererServicesImpl` (`CRenderer::offendingObject = const_cast<CObject *>(static_cast<const CObject *>(h.ptr));`), the four test classes and the TEMPORARY blocks of `CContractServices` and `CContractForwarder` (Q11, A2); `ShadingContract_OffendingObject` turns green.
- [X] T132 [US7] Remove `#include "surface.h"` from `src/libshader/shading/execute.cpp` and `executeMisc.cpp`; add `#include "attributes.h"` to `execute.cpp` (Shading -> Graphics State, allowed;
  `executeMisc.cpp` already has it). Confirm `grep -n '#include "\(surface\|object\)\.h"' src/libshader/shading/execute*.cpp` is empty, and that no transitive path reaches `object.h`/`surface.h` from them (`bundles.h`
  -> `ray.h` and `shading.h`; `irradiance.h`; `photonMap.h`). Record any residual edge: it is a stop condition, not an allowlist addition.
- [X] T133 [US7] [RUN] `ShadingContract_OffendingObject` is green, and CP-c: `ctest -L architecture` now passes with `ALLOWED DB-006 x 31`, `DB-001 x 8`, `DB-005 x 8`, `RESULT PASS`, no `VIOLATION`, no `STALE`; the Shading metric still 0/0.
- [X] T134 [US7] [RUN] Checkpoint 7: CP-a, CP-b, CP-c (8 / 8 / 31 as the table says), **CP-d**, CP-m, the scope greps of quickstart §3 for SC-002 (`grep -n 'currentObject->attributes' src/libshader -r`: only in `shading.cpp`,
  the sites of research R5's table; `grep -n 'currentObject->xform' src/libshader -r`: none). Record in validation.md §1.
- [X] T135 [US7] [RUN] CP-L for Phase 7, with Linux's own CP-c (8 / 8 / 31).
- [X] T136 [US7] **Checkpoint 7 (PAUSE 7)**: stage; summarize (DB-006 33 -> 31; `shading.cpp -> object.h` stays; the 7 + 1 reads and where each stays); CP results; wait for review.


---

## Phase 8: The tracer (US6; FR-010 to FR-012)

**Goal**: Shading's 10 trace call sites and `irradianceDispatch.cpp`'s 2 go through `ITracer`; the observer moves into the helpers; the replay stays
green unchanged. Red: 3 DB-001 `VIOLATION` lines, and the trace tests. Tests that go green: `TraceRequests{Interp,Jit}`, `GatherRequests{Interp,Jit}`,
`CannedTrace{Interp,Jit}`, `NoTracer`, `NoServices_Tracer`.

### Tests block

- [X] T137 [US6] Delete exactly these three lines from `tests/architecture/allowlist.txt`, **by key** (Q1): `DB-001  link  src/libshader/shading/executeMisc.cpp  ->  _ZN15CShadingContext5traceEP4CRay`;
  `DB-001  link  src/libshader/shading/executeMisc.cpp  ->  _ZN15CShadingContext7traceExEP10CRayBundle`; `DB-001  link  src/libshader/shading/shading.cpp  ->  _ZN15CShadingContext7traceExEP10CRayBundle`. The
  two bundle vtables, `CGatherBundle::CGatherBundle()` and the two `CPhotonMap::lookup` lines stay for spec 029.
- [X] T138 [US6] Test gate for Phase 8 (TG steps 1 to 3). Enable `ShadingContract_TraceRequests{Interp,Jit}`, `GatherRequests{Interp,Jit}`, `CannedTrace{Interp,Jit}`, `NoTracer` and `NoServices_Tracer`. [RUN] build and run
  them, and `ctest -L architecture`. Expected red: the tracer tests fail because the double's `tracer()` was never asked (and, in the standalone program, the TEMPORARY stub traps for `trace`/`traceEx` abort: an abort is red
  evidence); `NoServices_Tracer` fails for lack of the new-kind warning; `ctest -L architecture` reports **exactly the three `VIOLATION`s** of the deletion above. Record in validation.md §2.
- [X] T139 [US6] Edit the existing wrappers (QA, decided 2026-10-31; named for approval): add `ITracer *tracer() override { return inner->tracer(); }` to `CForwardingServices`, `CRecordingServices` and `CTextureServices` (not
  `CStubServices`: it has no inner implementation). Edit the new test code `load_stub.cpp`: **delete the two TEMPORARY traps** `CShadingContext::trace(CRay *)` and `traceEx(CRayBundle *)` (the table becomes the 4 symbols of
  contracts/test-doubles.md). That deletion is an edit to an approved test and is approved here.
- [X] T139a [US6] Remove `attachFakeObject()` from `tests/shading_contract/contract_harness.h` (and the `ri/geometry/object.h` include if nothing else uses it) and its two callers, `case_trace_requests.cpp` (`TraceRequests{Interp,Jit}`) and `case_no_services.cpp`
  (`NoServices_Tracer`): the trace path reads the object's attributes only through `objectAttributes()` since Phase 7 (T130), whose consumers take built-in defaults when it is null, so the standalone program runs the probe and bundle forms with a null current object (QE, as decided). This edits approved test code
  (approved at T064, 2026-10-31); show the diff in the Phase 8 report. If a read on the trace path could not be routed in Phase 7, keep `attachFakeObject()` for that read alone and name the read and why.
- [X] T140 [US6] **TEST-APPROVAL (PAUSE 8a)**: stage the allowlist deletion, the enabled tests, `T139`; summarize the red output, the three `VIOLATION`s, the stub change (6 -> 4 symbols), the removal of `attachFakeObject()` (`T139a`, with its diff) and the
  replay's role (the eight `TraceReplay_*` tests are **not** edited and **not** re-recorded; they must stay green through the move of the observer). **Wait for approval.**

### Implementation

- [X] T141 [US6] Create `src/libshader/shading/traceCalls.h` and `traceCalls.cpp` as contracts/tracer.md: `enum class TraceKind : uint8_t { Trace, Transmission, Visibility, Gather, Occlusion, IndirectDiffuse }`;
  `libshader::traceRay(CShadingContext &, CRay *, TraceKind)` and `traceBundle(CShadingContext &, CRayBundle *, TraceKind)`. Each calls `CShadingContext::traceRequestObserver` first when set (moved here from the call
  sites), then `context.getServices()->tracer()`. With no services or no tracer: warn once per process per kind (`std::atomic<bool>` per kind, `CODE_NOTSTARTED`), then `traceRay` sets `ray->object = nullptr` and
  `traceBundle` calls `bundle->postShade(bundle->numRays, bundle->rays)` (the miss path `traceEx` takes past the maximum ray depth, FR-012). Add `traceCalls.cpp` to `src/libshader/shading/CMakeLists.txt`. Includes
  `shading.h` and `tracer.h` only.
- [X] T142 [US6] Rewrite the 10 Shading call sites to go through the helpers, **removing the Phase 1 observer calls at them in the same edit** (the helpers observe the same thing): in `executeMisc.cpp`
  the probe `trace(cRay)` (kind `Transmission` or `Visibility`, chosen by the caller, `traceTransmission`'s probe) and the reflection probe `trace(cRay)` (`Trace`); the two `traceEx` in `traceTransmission`
  (`Transmission`) and the four in `traceReflection` (`Trace`); in `shading.cpp` the two `traceEx(lastGather)` in `gatherSample` (`Gather`). The ray-traced `environment()`/`shadow()` and the JIT's `jitTraceBatch`
  reach `traceReflection`/`traceTransmission`, so both backends are covered (FR-011).
- [X] T143 [P] [US6] In `src/ri/hiders/irradianceDispatch.cpp` replace the two `context->trace(&ray)` calls in `CIrradianceCache::sample` by `libshader::traceRay(*context, &ray, TraceKind::Occlusion)` and
  `(..., TraceKind::IndirectDiffuse)`, removing the Phase 1 observer calls there (Q6). Include `libshader/shading/traceCalls.h` (Hiders -> Shading, allowed). That file is built into `ribRender` only; `ribVector`
  uses `vector/irradianceDispatchStub.cpp`.
- [X] T144 [P] [US6] Create `src/ri/hiders/hiderTracer.h` and `hiderTracer.cpp`: `class CHiderTracer final : public ITracer` with `traceRay(CShadingContext &c, CRay *r) override { c.trace(r); }` and `traceBundle(...)
  override { c.traceEx(b); }`, stateless, with `static_assert(openrender::shading::kRendererServicesVersion.major == 2);`. Add `hiders/hiderTracer.cpp` to `ri_sources` in `src/ri/CMakeLists.txt` **and to the `REMOVE_ITEM`
  list for ribVector** with the other hider units (it calls `trace()`/`traceEx()`, which ribVector does not build).
- [X] T145 [US6] Install the tracer per frame. `src/ri/render/renderer.h` and `rendererStatics.cpp`: `static ITracer *frameTracer;` (`nullptr` between frames). `renderer.cpp` `beginFrame()`: `frameTracer =
  &hiderTracer;` (a function-local static `CHiderTracer`) right after `textureSystem` is created and **before** `beginFrameParams()` and before any context is constructed; `endFrame()`: `frameTracer = nullptr;` right
  after the contexts are deleted, next to `endFrameParams()`. `CRendererServicesImpl::tracer()` returns `CRenderer::frameTracer`. Put the three-point answer to FR-010 (one hider class per frame; the adapter is stateless
  and every piece of trace state is a member of the context argument; the slot is written only while no render thread exists, the imager runs inside the frame) in a comment at `frameTracer`. That answer is **argued from the code, not tested** (accepted by the maintainer, C6); say so in the comment.
- [X] T146 [US6] [RUN] Build and run the Phase 8 tests (all green) and, **unedited and not re-recorded, the eight `ShadingContract_TraceReplay_*` tests** (green: the observer now sits in the helpers and
  observes what is handed to the tracer). A red replay means a call site's rays changed: stop.
- [X] T147 [US6] [RUN] Re-measure the imports after the change: macOS `nm -m` and `dyld_info -fixups` of `libshader_shading` -> `libshader-imports-after-macos.txt`; expect **4** (`CGatherBundle::CGatherBundle()`, the two bundle
  vtables, `CPhotonMap::lookup`), a subset of the starting 6 (SC-011); the standalone program still starts with the 4-symbol stub. Check `grep -rn 'CShadingContext::trace' src/libshader` shows no reference.
- [X] T148 [US6] [RUN] Checkpoint 8: CP-a, CP-b, **CP-c** (`ALLOWED DB-001 x 5`, `DB-005 x 8`, `DB-006 x 31`, `RESULT PASS`, no `STALE`), **CP-d**, CP-m. Record in validation.md §1.
- [X] T149 [US6] [RUN] CP-L for Phase 8, including `nm -D --undefined-only` and `readelf -d` of `libshader_shading.so` -> `libshader-imports-after-linux.txt` (the Linux imports must not grow) and the Linux
  stub table at 4 symbols (plus whatever `BIND_NOW` requires, recorded).
- [X] T150 [US6] **Checkpoint 8 (PAUSE 8)**: stage; summarize (US6 done: `ITracer` is Shading's, the hiders answer it unchanged, DB-001 8 -> 5, imports 6 -> 4, replay unchanged); CP results; wait for review.


---

## Phase 9: Situations (US5; FR-017, FR-018)

**Goal**: the FR-017 table is checked against the code's present inference. Test that goes green: `ShadingContract_Situations`.

### Tests block

- [X] T151 [US5] Test gate for Phase 9 (TG steps 1 to 3). Enable `ShadingContract_Situations`. [RUN] build and run it. Remove `EXCLUDE_FROM_ALL` from `test_shading_contract_situations` too. Expected red: the executable does not build, because the observers' reports have no `dim`, `displaceOnly`,
  `inShadow`, `rayDepth`, `illuminationHook` fields yet (the build failure naming the missing fields is the red evidence; record it). Record in validation.md §2.
- [X] T152 [US5] **TEST-APPROVAL (PAUSE 9a)**: stage the enabled test; summarize the situation table's 14 rows, which rows are rendered (1 to 5, 8, 9) and which are asserted from `classifyShading()` alone (6 and 7: row 6 because its present behaviour is an oddity, #119, and row 7 kept as decided in B8 now that #116 is resolved by #129; and 10 to 14, which no `shade()` call reaches), the (call site, hit mode)
  pairs from `T009` and the seven remaining oddities the test asserts as oddities (the DLO vertex count, #116, was fixed by #129). The only existing-test edit in the phase is the trailing fields below (FR-023, allowed): existing readers ignore them; check by compiling
  the spec-039 tests that read the reports. **Wait for approval.**

### Implementation

- [X] T153 [US5] In `src/libshader/shading/shading.h` append the trailing fields `dim`, `displaceOnly`, `inShadow`, `rayDepth`, `illuminationHook` to `CPointKeyReport` and `CPointDisplacedReport`, and fill them at the
  existing observer call sites (`shade()`/`displace()` in `shading.cpp`, where `pointKeyObserver` is invoked) from the values already in scope. **Append only; do not reorder or rename a field.** The observers stay null in
  production; nothing new is computed unless an observer is set. Any spec-039 test that aggregate-initializes a report must still compile unchanged (if one does not, stop: it is an edit to an approved test).
- [X] T154 [US5] [RUN] Build and run `ShadingContract_Situations` (green: `classifyShading()` returns the table's row for every rendered situation and the branch `shade()` took matches the hit mode; rows 10 to 14 asserted
  from the function), then the spec-039 tests (`ctest -R 'Sampling|PointKey|Displaced'` or the labels they carry) unchanged and green.
- [X] T155 [US5] [RUN] Checkpoint 9: CP-a, CP-b, CP-c (5 / 8 / 31), **CP-d**, CP-m. Record in validation.md §1. Check #117 to #120 (O3 to O6, §4.3; O2 #116 is resolved by #129, closed 2026-11-01) against what the situation test showed; any extra evidence goes to the coordinator as a comment on the issue (not fixed here).
- [X] T156 [US5] [RUN] CP-L for Phase 9.
- [X] T157 [US5] **Checkpoint 9 (PAUSE 9)**: stage; summarize (US5 done: `ShaderGlobals`, the enumerations and the situation table exist and match the code; nothing in the render path consumes them, FR-018); CP results; wait for review.


---

## Phase 10: Guardrails, documentation, Linux and the sweep (US7; FR-019 to FR-021, FR-025, FR-029; SC-008 to SC-012; the final summary covers SC-001 to SC-014)

**Goal**: registry and allowlist header counts, DEVNOTES, the interface documentation for SC-012, the JIT-off build, SC-010 timing, the Linux
finished-tree validation and, last, the determinism sweep on both platforms.

### Tests block (registry, documentation and performance checks; red before the edits)

- [X] T158 [US7] Write `specs/026-shading-contract-v2/tools/scope_greps.sh` (exit 1 on any failed check, printing which): quickstart §3's greps (`currentObject->attributes` only in `shading.cpp`; `currentObject->xform`
  none; no `surface.h`/`object.h` include in `execute*.cpp`; only Shading/Infra includes in the new headers **and in `sloLoader.{h,cpp}`** (no `renderer*.h`, nothing under `src/ri/{render,hiders,geometry}`; B2); the removed methods have no caller: `findCoordinateSystemWithType`, `lookupUserOption`, `pixelXsamples`);
  `git diff 628d6747 -- tests/architecture/allowlist.txt | grep '^+[^+]'` shows **only** the two header-count comments (no line added, none re-keyed) and exactly the five deleted lines are gone; the allowlist
  section headers read `(5)` for DB-001 and `(31)` for DB-006; `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` has the DB-018, DB-006 and DB-001 texts of contracts/allowlist-end-state.md, leaves DB-005 untouched, moves no entry to
  Resolved, and the **lines this spec adds** contain no word that depends on the merge (`merge`, `merged`, a hash). The check reads only `git diff 628d6747 -- DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md | grep '^+' | grep -v '^+++'`: the file already contains "merge" (lines 15, 534, 539 at `d01da47`) and those are not this spec's (A1); `git diff --stat 628d6747 --` over the "Files 026 never edits" list is empty; `DEVNOTES.md` has the spec 026 row; `shaderFunctions.h` passes `ACCESSOR_INTERIOR` for every `incident` line and `ACCESSOR_EXTERIOR` for every `opposite` line, and so do `jitIncidentParameter` and `jitOppositeParameter` in `shading.cpp` (FR-029, B3); `validation.md` §4 holds the classification and the text of the seven filed issues (#115–#121).
- [X] T159 [P] [US2] Write `specs/026-shading-contract-v2/tools/timing_sc010.sh`: five interleaved series of the starting-tree `orender` and the finished-tree `orender` on `Visual_*` scenes heavy in message passing and
  coordinate systems (the `somewood`, `wood` and `blue_marble` families) and a `gather` scene, at the same thread count, printing per-scene median and spread (SC-010; spec 028's §0.5 method). A difference beyond the
  measured spread is a defect.
- [X] T160 [US7] [RUN] Run `scope_greps.sh` now: it must fail on the document checks (header counts, registry text, DEVNOTES row) and pass on the code checks. Record the output in validation.md §2.
- [X] T161 [US7] **TEST-APPROVAL (PAUSE 10a)**: stage the two scripts; summarize what each checks and the red output. These are the phase's tests; no existing test is edited. **Name for approval the two edits to architecture data that the implementation below makes**: the allowlist section-header counts (`T162`, `tests/architecture/allowlist.txt`) and, only if the final report differs, the lowered metrics baseline (`T164`, `tests/architecture/metrics-baseline.txt`). **Wait for approval.**

### Implementation: registry, allowlist and metrics (FR-019 to FR-021, SC-008, SC-009)

- [X] T162 [US7] Update the allowlist section-header comments in `tests/architecture/allowlist.txt`: `# --- DB-001: ... (8) ---` becomes `(5)` and `# --- DB-006: ... (33) ---` becomes `(31)`. No other change to the file.
- [X] T163 [US7] Edit `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` per contracts/allowlist-end-state.md, with "spec 026" and today's date and no reference to the merge. **DB-018** (stays Open): evidence says the eight builtins ask
  `getAttribute`/`getMessage` and none reads `currentObject->attributes`; "still open": the JIT's message-passing entry points (`op_*_param`, `op_lightsource_f`: spec 033), `shadername(type)` (Q8), group-layer messages and `setmessage`
  (spec 034), the `lightsource()` backend divergence (filed as #115) and the unknown destination size of a float-class query (filed as #121); direction drops the parts done. **DB-006** (stays Open): size 33 -> 31 (26 include -> 24, 7 link); history line "Spec 026, <date of this task>: `execute.cpp` and `executeMisc.cpp` no longer
  include `surface.h` ...; `shading.cpp -> object.h` stays for spec 029. The `ShaderGlobals` contract exists (`shaderGlobals.h`); nothing fills it yet."; evidence "Shading includes `object.h` (one file)". **DB-001** (stays Open): evidence 4 imports;
  size 8 -> 5; the history line of contracts/allowlist-end-state.md; direction names `ITracer` as the interface the bundles and the photon map follow (spec 029). **DB-005**: not edited. No entry moves to Resolved. Use the date on which the task is done (the history lines carry that date, not the date of the design).
- [X] T164 [US7] [RUN] Compare the final `METRIC` lines of `archcheck` with `tests/architecture/metrics-baseline.txt` (Shading `CRenderer::` references 0/0 and no rise). Expected: no change. If a figure is lower, set the baseline to the
  measured value with a comment in the file's house style; **never raise it** (FR-021).
- [X] T165 [P] [US7] Add the spec 026 row to the status table of `DEVNOTES.md` in the style of the rows for specs 024, 025, 027 and 028: what moved (the v2 services, `ITracer`, `ShaderGlobals`, the `.slo` loader into Shading), the registry deltas (DB-001 8 -> 5, DB-006 33 -> 31, DB-018's service part done), "no image changed", the
  validation platforms and dates (filled in by `T174`), and the open findings (pointing at `validation.md` §4). No text that depends on the merge.

### Implementation: documentation (FR-001, FR-002, SC-012)

- [X] T166 [US1] Complete the documentation of `src/libshader/shading/rendererServices.h`: for **each of the remaining methods** (the 28 unchanged and `setOffendingObject`) the inputs, outputs, not-found result, thread-safety, whether it
  may re-enter Shading and the version that introduced it ("pre-2.0.0, unchanged"); bring the version history table up to the final set (what was removed, what was added, what was replaced). An implementer who reads only the headers must
  be able to write a conforming double.
- [X] T167 [US1] **Ask the maintainer** to name the SC-012 reviewer (someone who did not write the headers or the double) and to confirm the review method: the reviewer reads `rendererServices.h`, `serviceTypes.h`, `tracer.h`,
  `shaderGlobals.h` and compares them with `tests/shading_contract/contract_services.h`. Record the answer in validation.md §5. The implementer does not review its own work and does not mark SC-012 met.
- [X] T168 [US7] **Ask the maintainer** whether to add the spec 026 entries to `CLAUDE.md` (a gotcha 27 on the v2 services, the tracer's per-frame slot and the stub; Recent Changes; Active Technologies, as specs 024 to 028 did). If
  authorized in words, edit only those parts and **never the "Spec-kit agent rules" section**; if not, do nothing and record "CLAUDE.md untouched" in validation.md §5.

### Implementation: builds and measurements

- [X] T169 [US2] [RUN] The JIT-off build (quickstart §4c), once, on macOS, in its own tree: check `df -h /Volumes/Projects`, `$CJ 026-nojit-configure cmake -B build-nojit -S . -DCMAKE_BUILD_TYPE=Release -DOPENRENDER_ENABLE_JIT=OFF` (reused from `T079`: a re-configure, then an incremental build), `$CJ 026-nojit-build cmake --build
  build-nojit --config Release -j4`, `$CJ 026-nojit-suite ctest --test-dir build-nojit -LE determinism-sweep -j4 --output-on-failure`. It builds under `-Werror`, the `*Jit` contract tests and the `.slo` halves are not registered, and the suite has
  no failure outside the known-flaky set. If it has one, report it and ask the maintainer whether to take the starting tree's JIT-off baseline from `build-base-src` (`T170`) with a second configuration (an extra build; not done
  unasked). Then `rm -rf build-nojit`.
- [X] T170 [US2] [RUN] Build the starting tree for SC-010 (and for the JIT-off comparison): `mkdir build-base-src && git archive 628d6747 | tar -x -C build-base-src`, configure and build `build-base` from it (Release, JIT on), `-j4`.
  This is a plain source export: no git state is touched. Keep it until `T171` has run.
- [X] T171 [US2] [RUN] SC-010: run `tools/timing_sc010.sh` (finished tree `build` against `build-base`), five interleaved series, one job at a time under `cpujob`. Record the table in validation.md §1; a regression beyond the measured spread is a defect
  to report, not a threshold to move. Then `rm -rf build-base build-base-src`.
- [X] T172 [US6] Record SC-011 in validation.md §1: macOS imports base (6) -> after (4), a subset; Linux imports base -> after, not grown (the files from Phases 1 and 8).

### Final checkpoint and the sweeps

- [X] T173 [US2] [RUN] Final macOS checkpoint: `scope_greps.sh` now exits 0; CP-a, CP-b, **CP-c** (`ALLOWED DB-001 x 5`, `DB-005 x 8`, `DB-006 x 31`, `RESULT PASS`), **CP-d**, CP-m (every contract test green, none disabled); `ctest -L shading_parity`, `-L texture_tile -j2`,
  `-L '^determinism$' -j2`. Record in validation.md §1.
- [X] T174 [US2] [RUN] Final Linux checkpoint (CP-L): push, verify, build at `-j2` under GCC 13 `-Werror`, the full non-sweep suite, CP-d against `-linux`, CP-c (5 / 8 / 31), CP-m, `scope_greps.sh` (on the pulled tree, or on the remote with `git` unavailable the code
  checks only), `-L '^determinism$' -j2`. Record in validation.md §3, and the dates in the DEVNOTES row.
- [X] T175 [US7] Check the state before the long runs: `git status` shows every changed file staged and nothing unintended (`build*`, `build-logs`, scratch trees are ignored), `git diff --stat 628d6747` over the "Files 026 never edits" list is empty,
  and no GitHub write was made.
- [X] T176 [US2] [RUN] **The determinism sweep on macOS, last**: first `$T/image_manifest.sh stamp build` (N2: the manifest diff reads only images newer than the stamp), then `$CJ 026-sweep ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure` (413 scenes x 5 configurations, about 10 to 12 minutes at `-j2`). Record the result. Any red scene is re-run alone before it is written off. **Then diff against the starting tree's sweep manifest (B1, SC-007)**: `$T/image_manifest.sh diff $T/baseline-sweep-images-macos.sha256 build $T/baseline-sweep-noise-macos.txt` must report 0 differing and 0 missing images (the stamp is taken just before the run, as in `T007`). A difference stops the work for the maintainer.
- [X] T177 [US2] [RUN] **The determinism sweep on Linux, last**: first `ssh "$H" "cd $RR && specs/026-shading-contract-v2/tools/image_manifest.sh stamp build"` (N2), then `ssh "$H" "cd $RR && ~/.claude/bin/cpujob 026-linux-sweep ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure"`. Record the result in validation.md §3; compare the failing set with Linux's baseline failing set from Phase 1, and **diff against the Linux sweep manifest** (B1, SC-007): `$T/image_manifest.sh diff $T/baseline-sweep-images-linux.sha256 build $T/baseline-sweep-noise-linux.txt` on the remote tree (after the stamp and the run), 0 differing and 0 missing.
- [X] T178 [US7] **Final checkpoint (PAUSE 10)**: stage every changed file with `git add` (never commit), confirm `git status` shows no unstaged change and no build artefact, fill validation.md §5 against SC-001 to SC-014 (SC-012 is the reviewer's to mark),
  confirm F1 to F4 are closed by their tests (list the T-numbers), list the evidence added as comments on #115 to #121 (O1 to O7, §4.3), add the Linux failures that predate this spec as a separate note, summarize, and wait for review.
  Then stop; the coordinator decides the next stage (converge). #32 is closed by the maintainer's `Closes #32` line in the merge commit, not by any task.


---

## Dependencies & Execution Order

### Phase dependencies

| Phase | Depends on | Notes |
|---|---|---|
| 1 Setup, baselines, replay, Linux gate | nothing | the only `src/` edit is the null-in-production observer; the replay is recorded here and then frozen |
| 2 Tests first | 1, **including the released Linux gate** (`T042`) | no `src/` edit; one TEST-APPROVAL for the bulk of the test code |
| 3 Foundation | 2 | from here the tree cannot build the starting `src/` with the new tests; the contract programs turn on |
| 4 `getMatrix` (US4) | 3 | removes 2 methods; the first wrapper forwarding override |
| 5 `getAttribute` (US3) | 3 (ordered after 4 by the plan) | removes 26 methods; adds the expected-type argument to the interpreter macro; F1 (attributes), F2 (attributes), F4 |
| 6 `getMessage` (US3) | 3 and 5 (`T101`) | `lightsource()` keeps each backend's behaviour; F1 (messages), F2 (messages, `shadername`), F3 |
| 7 DB-006 | 3 (ordered after 4 to 6, so that only research R5's reads are left) | deletes 2 DB-006 lines first |
| 8 Tracer (US6) | 7 (ordered after it, so that the archcheck delta is only DB-001's) | deletes 3 DB-001 lines first; shrinks the stub 6 -> 4 |
| 9 Situations (US5) | 3 (independent of 4 to 8 in code; ordered after 8 by the plan) | trailing report fields only |
| 10 Guardrails, docs, Linux, sweeps | 9 | `T167` and `T168` ask the maintainer and can be answered at any earlier pause |

Phases 4, 5 and 6 are three separate services and may be reordered; Phase 6 needs Phase 5's macro argument.

### Within each phase

- Phase 1: `T001` -> `T004` -> `T005` -> `T006` -> `T007` (before the hook); `T002`, `T003`, `T009`, `T010` [P] any time after `T001`; `T008` after `T001`; replay tests `T011` -> `T012`, `T013`, `T014` [P] ->
  `T015` -> `T016`; then `T017` -> `T018`, `T019` (`T017` first), `T020` [P] -> `T021` -> `T022` -> `T023` -> `T024` -> the eight mutations in order -> `T033` -> `T034`; the Linux tasks `T035` -> `T036` -> `T037` -> `T038` ->
  `T039` -> `T040` -> `T041` -> `T042` need `T023` and `T010`; `T043` last.
- Phase 2: `T044`; then `T045`, `T046`, `T047`, `T048`, `T049`, `T050`, `T051`, `T052`, `T053`, `T054`, `T055`, `T056`, `T057`, `T058`, `T059`, `T060`, `T061` [P] (different files; the cases need
  the double, the harness and the shaders to compile, not to be finished first) -> `T062` -> `T063` -> `T064`.
- Phase 3: `T065` -> `T066` (pause) -> `T067` -> `T068`, `T069` [P] -> `T070` (needs `T069`) -> `T071` -> `T072`; `T073` and `T074` after `T071` -> `T075` -> `T076` -> `T077` -> `T078` -> `T079` -> `T080` -> `T081`.
- Phases 4, 5, 6: (Phases 5 and 6: the new F-tests `T092`, `T093`, `T094` [P] and `T109`, `T110` [P] first) -> Test gate -> wrapper edits -> TEST-APPROVAL (pause) -> the renderer's implementation -> Shading's side (and the JIT mapping / macro) -> the interface removal -> green run -> checkpoint -> Linux -> pause.
- Phase 7: `T124` -> `T125` -> `T126` -> `T127` -> `T128` (pause) -> `T129` -> `T130`, `T131` -> `T132` -> `T133` -> `T134` -> `T135` -> `T136`.
- Phase 8: `T137` -> `T138` -> `T139` -> `T140` (pause) -> `T141` -> `T142`, `T143` [P], `T144` [P] -> `T145` -> `T146` -> `T147` -> `T148` -> `T149` -> `T150`.
- Phase 9: `T151` -> `T152` (pause) -> `T153` -> `T154` -> `T155` -> `T156` -> `T157`.
- Phase 10: `T158`, `T159` [P] -> `T160` -> `T161` (pause) -> `T162`, `T163`, `T165`, `T166` -> `T164`; `T167`, `T168` any time; `T169`, `T170` -> `T171`, `T172` -> `T173` -> `T174` -> `T175` -> `T176` -> `T177` -> `T178`.

### User story map

| Story | Tasks |
|---|---|
| US1 (Shading has one documented, versioned, test-doubled boundary) (25) | T008, T044, T045, T047, T048, T051, T056, T060, T062, T064, T065, T066, T067, T071, T072, T073, T074, T075, T076, T077, T078, T080, T081, T166, T167 |
| US2 (renders do not change) (17) | T004, T005, T007, T022, T039, T040, T043, T063, T079, T159, T169, T170, T171, T173, T174, T176, T177 |
| US3 (message passing and attribute queries go through the services) (37) | T049, T052, T053, T058, T092, T093, T094, T095, T096, T097, T098, T099, T100, T101, T102, T103, T104, T105, T106, T107, T108, T109, T110, T111, T112, T113, T114, T115, T116, T117, T118, T119, T120, T121, T122, T123, T127 |
| US4 (coordinate systems come from one service) (12) | T050, T054, T082, T083, T084, T085, T086, T087, T088, T089, T090, T091 |
| US5 (a shading point has a contract with a ray type and hit mode) (12) | T009, T057, T061, T069, T070, T151, T152, T153, T154, T155, T156, T157 |
| US6 (tracing goes through a Shading-owned interface) (43) | T011, T012, T013, T014, T015, T016, T017, T018, T019, T020, T021, T023, T024, T025, T026, T027, T028, T029, T030, T031, T032, T033, T038, T041, T046, T055, T059, T068, T137, T138, T139, T140, T141, T142, T143, T144, T145, T146, T147, T148, T149, T150, T172 |
| US7 (the registry reflects what moved) (25) | T006, T034, T037, T124, T125, T126, T128, T129, T130, T131, T132, T133, T134, T135, T136, T158, T160, T161, T162, T163, T164, T165, T168, T175, T178 |
| Cross-cutting (no label) (7) | T001, T002, T003, T010, T035, T036, T042 |

### Phase summary

| Phase | Tasks | First ID - last ID | TEST-APPROVAL | PAUSE |
|---|---|---|---|---|
| 1 | 43 | T001 - T043 | T016 | T043 |
| 2 | 21 | T044 - T064 | T064 | T064 (the TEST-APPROVAL is also the phase pause) |
| 3 | 17 | T065 - T081 | T066 | T081 |
| 4 | 10 | T082 - T091 | T084 | T091 |
| 5 | 17 | T092 - T108 | T097 | T108 |
| 6 | 15 | T109 - T123 | T113 | T123 |
| 7 | 13 | T124 - T136 | T128 | T136 |
| 8 | 14 | T137 - T150 | T140 | T150 |
| 9 | 7 | T151 - T157 | T152 | T157 |
| 10 | 21 | T158 - T178 | T161 | T178 |
| **Total** | **178** | | | |

### Requirement traceability

| Requirement | Tasks |
|---|---|
| FR-001, FR-002 (one versioned, documented interface) | `T071`, `T067`, `T166`, `T167` |
| FR-003 (no renderer-core, hider or geometry type) | `T067`, `T068`, `T069`, `T071`, `T131`, `T078`, `T158` |
| FR-004 (standalone double) | `T045`, `T051`, `T077` |
| FR-005 (`get_attribute`) | `T052`, `T098`, `T099`, `T100`, `T102`, `T103`, `T105` |
| FR-006, FR-007 (`getmessage`, both backends) | `T053`, `T114`, `T116`, `T117`, `T118`, `T101`, `T120` |
| FR-008, FR-009 (`get_matrix`) | `T054`, `T085`, `T086`, `T087`, `T088` |
| FR-010 to FR-012 (`ITracer`) | `T068`, `T046`, `T055`, `T059`, `T141`, `T142`, `T143`, `T144`, `T145`, `T146` |
| FR-013 (texture part unchanged) | `T071` (no texture member touched), `T078`, `T173` |
| FR-014 (28 unchanged, 1 replaced, 28 removed) | `T008`, `T087`, `T104`, `T131`, `T158` |
| FR-015, FR-016 (`ShaderGlobals`, absence rules) | `T069`, `T057`, `T077` |
| FR-017, FR-018 (ray type, hit mode, table; rows 6, 7, 10-14 from the function alone, B8) | `T070`, `T061`, `T153`, `T154`, `T009` |
| FR-019 (architecture, shrink only) | `T125`, `T133`, `T137`, `T148`, `T162`, `T158`, `T173` |
| FR-020 (registry) | `T163`, `T158` |
| FR-021 (metrics) | `T164` |
| FR-022 (no output change) | `T004`, `T005`, `T007`, `T040`, `T022`, `T078`, `T089`, `T106`, `T121`, `T134`, `T148`, `T155`, `T173` |
| FR-023 (tests first, shown failing) | `T016`, `T064`, `T066`, `T084`, `T097`, `T113`, `T128`, `T140`, `T152`, `T161`, the Test gates, `T025` to `T032` |
| FR-024 (stub, not hang) | `T047`, `T076`, `T139`, `T147` |
| FR-026 (type-safe copies, F1) | `T092`, `T093`, `T098`, `T102`, `T109`, `T110`, `T115`, `T116` |
| FR-027 (no current object, F2) | `T093`, `T110`, `T102`, `T116`, `T119` |
| FR-028 (`lightsource()` outside a loop, F3) | `T109`, `T117` |
| FR-029 (accessor constants, F4) | `T094`, `T101`, `T158` |
| FR-025 (no merge dependence) | standing rules, `T163`, `T158`, `T175` |
| SC-001 (standalone program) | `T051`, `T077` |
| SC-002 (zero object-attribute reads for the builtins) | `T130`, `T134`, `T158` |
| SC-003, SC-004 (identical requests; `get_matrix`) | `T052`, `T053`, `T054`, `T088`, `T105`, `T120` |
| SC-005 (same rays as master) | `T023`, `T024`, `T025` to `T032`, `T146` |
| SC-006 (situations) | `T061`, `T154` |
| SC-007 (byte-identical, same pass/fail set) | every CP-d, `T007`, `T040`, `T176`, `T177` |
| SC-008, SC-009 (architecture, registry) | `T133`, `T148`, `T173`, `T163` |
| SC-010 (timing) | `T170`, `T159`, `T171` |
| SC-011 (imports do not grow) | `T006`, `T041`, `T076`, `T147`, `T149`, `T172` |
| SC-012 (documentation and independent review) | `T166`, `T167` |
| SC-013 (no write past a destination; accessor constants agree) | `T094`, `T101`, `T092`, `T093`, `T109`, `T110`, `T105`, `T120` |
| SC-014 (null object and no-light tests) | `T093`, `T109`, `T110`, `T120` |
| FR-003 / Q11 (`setOffendingObject(ObjectHandle)` reaches the double) | `T124`, `T131` |
| FR-026 exemption for source `Light` (B4) | `T115`, `T117`, `T118` |
| FR-002 / SC-011 / B2 (the `.slo` loader moves with its guard; JIT-off compiles) | `T073`, `T076`, `T078`, `T079`, `T169` |

## Parallel opportunities

| Phase | Parallel tasks |
|---|---|
| 1 | `T002`, `T003`, `T009`, `T010`; `T012`, `T013`, `T014`; `T020` with `T018` |
| 2 | all test files from `T045` to `T061` (different files); the CMake task `T044` first |
| 3 | `T068` with `T069`; `T073` with `T074` |
| 4 to 7 | none worth running in parallel: each is one chain through shared files (`shading.cpp`, `rendererShadingServices.cpp`) |
| 8 | `T143` and `T144` with `T142` (different files) |
| 10 | `T158` with `T159`; `T165` with `T166` |

Builds and tests themselves never run in parallel: `cpujob` serializes them machine-wide.

## Parallel example: Phase 2 tests first

```text
T045 tests/shading_contract/contract_services.h
T046 tests/shading_contract/recording_tracer.h
T047 tests/shading_contract/load_stub.cpp
T048 tests/shading_contract/contract_harness.h
T052 tests/shading_contract/case_attributes.cpp
T053 tests/shading_contract/case_messages.cpp
T054 tests/shading_contract/case_matrices.cpp
T055 tests/shading_contract/case_trace_requests.cpp
T057 tests/shading_contract/case_globals_double.cpp
T058 tests/shading_contract/case_real_answers.cpp
```

## Implementation strategy

**MVP is Phases 1 to 6 (User Stories 1, 2, 3 and 4).** After Phase 6 the shading library runs against a standalone, versioned double with no renderer in the process, the eight message and attribute builtins and every
coordinate-system lookup ask the services from both backends, the 28 removed methods are gone (2 in Phase 4, 26 in Phase 5) and `setOffendingObject` is the one left to change (Phase 7), and every image of the suite is byte-identical to the starting tree's. The rest is incremental, one
concern per phase, each ending in a byte-identical checkpoint and a pause:
- Phase 7 removes the two DB-006 includes (US7), Phase 8 puts the tracer behind `ITracer` and deletes the three DB-001 lines (US6), Phase 9 proves the `ShaderGlobals` situation table against the code (US5).
- Phase 10 lowers the counts, updates the registry and the interface documentation, builds without the JIT, times SC-010, validates the finished tree on Linux and runs the determinism sweep on both platforms last.

The Linux baseline is a hard gate between Phase 1 and Phase 2: after Phase 3 begins, the starting tree can no longer be built with 026's tests, so it cannot be taken later. Each phase leaves the tree green at its checkpoint, so any checkpoint PAUSE is a
safe stopping point (a TEST-APPROVAL pause is not: the phase's tests are red there by design). Decisions QA to QG and the triage of the findings are taken (spec.md, Clarifications, "Session 2026-10-31"; `validation.md` §4).

