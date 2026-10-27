---

description: "Task list for spec 025: graphics state handles"
---

# Tasks: Graphics State Handles

**Input**: design documents in `specs/025-graphics-state-handles/`.

**Prerequisites**:
- plan.md (phase order, "Order of work")
- spec.md (US1–US5, FR-001 to FR-025, SC-001 to SC-010)
- research.md (R1–R13)
- data-model.md
- contracts/state-handles.md, attribute-scope.md, shader-parameter-hooks.md, channel-registry.md, state-vocabulary.md
- quickstart.md, validation.md (macOS baselines, §0), `tools/` (FR-025 scan data)

**Tests**: required. Constitution III (TDD) is non-negotiable. Phase 2 writes every new test first and shows
it red. The only tests that are green on the starting tree by design are:
- `ShadingParity_ShaderParameterEquivalence` part A (the API-neutral oracle, 64/64);
- `RibOut_ShaderStatementOrder` (a characterization test, clarification Q3);
- the existing guards.

`State_LightOrder` is written and red already (4 of 9 checks fail on `dfffba0`). It is the **only
allowed red test from Phase 2 until Phase 7**.

**Organization**: the phases follow plan.md's "Order of work" exactly. User-story labels are on the tasks
that serve a story, including the Phase 2 test tasks, so each story's tasks can be found by label.
Setup, checkpoint and documentation tasks carry no label. Every phase ends in a **checkpoint task** and then
a review pause: the agent stages its changes with `git add`, summarizes, and waits for the maintainer.

**Never commit.** Stage with `git add` only. Decline any spec-kit auto-commit hook.

**Formatting**: run clang-format with the repo's `.clang-format` on an edited file only if it was clang-format clean before the edit (check with `clang-format --dry-run` on the unedited file first); otherwise leave the formatting alone.

**Working directory**: `/Volumes/Projects/Development/CLI/openrender-worktrees/025-graphics-state-handles`
(branch `025-graphics-state-handles`, base `dfffba0`). The macOS build tree is `build-025`. Paths below are
repo-relative. Work only in this worktree.

---

## Standing rules (apply to every task)

**Builds and tests (marked [RUN])**:
- The implementer asks the coordinator before each [RUN], then runs it through `~/.claude/bin/cpujob <label> <command>`:
  one job at a time machine-wide, `nice -n 19`, at most `-j4`; the determinism sweep and other long runs use `-j2`.
- Long jobs go in the background with output redirected to `build-025/logs/<label>.log`. Wait for the completion notice.
- Never run a test binary directly (a hung one has no timeout). Never use `-j8`, a bare `-j`, or a count above 4.
- A plain `cmake --build` / `ctest` line below always means the `cpujob` form.

**Checkpoint procedure** (referenced as CP-a to CP-d by every phase's last task):
- **CP-a build**: `cpujob 025-pN-build cmake --build build-025 --config Release -j4`. It must be clean under `-Werror`.
- **CP-b suite**: `cpujob 025-pN-suite ctest --test-dir build-025 -LE determinism-sweep -j4 --output-on-failure`.
  - Before the run, stamp the build tree so only fresh images count: `specs/025-graphics-state-handles/tools/image_manifest.sh stamp build-025`.
  - Expected: the same pass list as T003's baseline, plus the new tests that are enabled. `State_LightOrder` red (4 of 9
    checks) is the only allowed failure before Phase 7. Tests registered `DISABLED` show as "Not Run (Disabled)" and are
    not failures (verify this the first time, in T019; if ctest's exit code counts them, stop and report).
  - A red test from the known-flaky set (memory `reference_ctest_flaky_tests.md`: `ImagerRegressionRender`,
    `Parity_transparency-matte-aov`, `Parity_depthdefault`, `ShadingParity_AmbientAccumulation`) is re-run alone
    before it is written off. `ShadingParity_AmbientAccumulation` is a real heap bug: re-run it under the same
    conditions as T003 and compare.
- **CP-c architecture**: `cpujob 025-pN-arch ctest --test-dir build-025 -L architecture --output-on-failure`, plus the full
  report from `build-025/tests/architecture/archcheck --source-dir . --build-dir build-025 --config tests/architecture`.
  Record `ALLOWED`, `METRIC` and `RESULT` lines in validation.md.
  - **Why the State-only targets are behind an option**: archcheck's `loadMakefiles` walks every `DependInfo.cmake`, `EXCLUDE_FROM_ALL`
    targets included, and exits 2 "incomplete build tree" when an in-scope object (any source under `src/`) is missing. The State-only
    library compiles `src/ri/state/*.cpp`, so it exists in the build tree only behind the CMake option `OPENRENDER_STATE_ONLY_TESTS`,
    **off until T050**. Until then CP-c sees only the production units. After T050 the option is on in `build-025` and the library is built
    (part of `all`), so archcheck finds every object (research R11).
- **CP-d byte-identical output** (Phases 3–6 only): `specs/025-graphics-state-handles/tools/image_manifest.sh diff
  specs/025-graphics-state-handles/tools/baseline-images.sha256 build-025 specs/025-graphics-state-handles/tools/baseline-image-noise.txt`
  must report **0 differing and 0 missing images**. Also confirm every `MaxBlockAvgDiff` in the CP-b log equals the baseline log's.
  Any difference stops the phase: it is a behaviour change in a behaviour-preserving phase (FR-022).

**Allowlist rules (FR-019)**: the only allowlist change is deleting the 7 DB-008 lines (T030) and any line the check
reports `STALE`. **Adding or swapping a line is never allowed.** If the check reports a new `VIOLATION`, the design is
wrong: stop and report. `metrics-baseline.txt` only goes down.

**Thresholds and references**: never widen a threshold, never disable a test, never regenerate a reference image to get a
green result. Only Phase 7 may change output, and only the maintainer may approve a reference re-render (T077).

**No task depends on the merge**: no post-merge step, no merge hash, no GitHub comment. DB-008 moves to Resolved with
"spec 025" and the date the task is done. DB-021 stays Open. The `Closes #31` line is the maintainer's to write in the merge commit.

**Stop conditions** (stop and report, never work around):
- a Geometry, Texturing or Scene Interface unit needs a Shading, Hiders or Display header (R12);
- archcheck mishandles a doubly compiled State unit (R11);
- a phase 3–6 image or `MaxBlockAvgDiff` differs from the baseline;
- the architecture check reports a new `VIOLATION`.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency on unfinished tasks).
- **[Story]**: US1–US5 from spec.md (US1 no cross-domain dependency, US2 renders as before, US3 last-bound record, US4 pattern table, US5 channel registry).
- **[RUN]**: a build or test run done by the implementer, one job at a time.

## Red-first convention for tests that cannot compile yet

A new test that needs an API that does not exist yet is added with `EXCLUDE_FROM_ALL` on its executable **and**
`set_tests_properties(<name> PROPERTIES DISABLED TRUE)` on its test, so the main build and every checkpoint stay green.
- The red state is shown by `cmake --build <build-dir> --target <exe>` failing to compile; the error text goes in validation.md.
- The task that provides the API removes both markers, builds, and runs the test green.
- **The State-only library and the five `State_*` State-only executables are different** (T007). They compile `src/ri/state/*.cpp` a second time on a deliberately narrow include path, and archcheck exits 2 on any missing in-scope object. So they are defined only when the CMake option `OPENRENDER_STATE_ONLY_TESTS` is on, **off by default until T050**. Their red evidence is taken in a scratch build directory with the option on (T019). T050 switches it on (default ON from then, and explicitly in `build-025`) together with the enabling edits.
- The vocabulary test, the Direct equivalence test and the bxdf-ignored test compile only `tests/` sources, so they use the plain `EXCLUDE_FROM_ALL` + `DISABLED` form in every configuration.

---

## Phase 1: Setup — baseline record (before ANY source edit)

**Purpose**: fix the numbers every later checkpoint is compared against, and create the tool that proves
"byte-identical output" for Phases 3–6. The macOS baselines of the plan stage are in validation.md §0.
These tasks re-take the ones the checkpoints need, on the tree as it stands (the two new test files are in it).

- [X] T001 Append an "Implementation log" section (§1 onward) to `specs/025-graphics-state-handles/validation.md`.
  - Record the base commit `dfffba0`, the worktree HEAD (`git rev-parse HEAD`), and `build-025`.
  - Add one table row per phase for the checkpoint results (build, suite totals, architecture report, image diff).
  - State the expected-red rule: `State_LightOrder` only, until Phase 7.
- [X] T002 [P] Write `specs/025-graphics-state-handles/tools/image_manifest.sh` (bash, `shasum -a 256`; must also run with `sha256sum` on Linux). It discovers outputs generically, so it needs no list of test directories and does not depend on a first run:
  - `stamp <build-dir>` touches `<build-dir>/.image_stamp`;
  - `record <build-dir>` prints `sha256  relative-path` for every `*.tif`, `*.png` and `*.exr` under `<build-dir>/tests` that is **newer than the stamp** (so a stale file from an earlier run, or a copied fixture, is never hashed);
  - `diff <manifest> <build-dir> [noise-list]` reports every image that differs or is missing (in the manifest but not fresh), and lists fresh images that are not in the manifest as information only; paths in the noise list are ignored; exit 1 on any difference or missing image.
- [X] T003 [RUN] Baseline on the unchanged tree (the new test files are in it): `image_manifest.sh stamp build-025`, then
  CP-a, CP-b (`cpujob 025-base-suite …`) and CP-c.
  - Record the totals, every failing test, and which of them are known-flaky.
  - Expected: `State_LightOrder` red (4 of 9) is the only failure; `ALLOWED DB-008 x 7`; architecture PASS.
  - validation.md §0.3 and plan.md report 784/784 passed for the suite as it stood before `State_LightOrder` (which is red) was in the build. Take the new total from this run and state in the log which tests it includes; do not compare it with 784.
  - Save the `MaxBlockAvgDiff` lines of the log as `specs/025-graphics-state-handles/tools/baseline-diffs.txt` (CP-d compares them).
- [X] T004 [RUN] Directly after T003: `image_manifest.sh record build-025 > specs/025-graphics-state-handles/tools/baseline-images.sha256`.
- [X] T005 [RUN] Run-to-run noise on the unchanged tree: `image_manifest.sh stamp build-025`, run CP-b a second time, `record` again, and list every
  path whose hash differs from T004 in `specs/025-graphics-state-handles/tools/baseline-image-noise.txt` (expected: empty, since the plan stage saw 95/95 identical).
  Note any listed image in validation.md. A file in the noise list is excluded from CP-d, but its `MaxBlockAvgDiff` must still match.
- [X] T006 **Checkpoint 1 (PAUSE 1)**: the baselines are recorded. Stage `validation.md` and the four new `tools/` files (`image_manifest.sh`, `baseline-images.sha256`, `baseline-image-noise.txt`, `baseline-diffs.txt`); summarize; wait for review.
- [X] T006a Maintainer decision 2026-10-26, after Checkpoint 1: fix the `tests/imager/ri.tif` race found by T005, and widen the manifest.
  - **Race fix** (`tests/imager/CMakeLists.txt` only; no test source and no assertion changes): `ImagerGuardTests` (its Test 5b renders with no `Display`) now runs in its own working directory `tests/imager/guard`, so its default `ri.tif` no longer lands on the one `ImagerExecutionTests` writes at `tests/imager/ri.tif`. `ImagerExecutionTests` keeps its output name and directory. This edit to an approved test is authorized by the maintainer for exactly this purpose.
  - **Manifest**: `image_manifest.sh record` also hashes `<build-dir>/imager_*_test.tif` (the four `Imager*Render` outputs at the build root).
  - Re-record `baseline-images.sha256` and `baseline-diffs.txt` with the fixed test, re-run the noise check (stamp, CP-b, `diff`; it is now empty), and empty `baseline-image-noise.txt` of its `ri.tif` line. Record it in validation.md §1.4.

---

## Phase 2: Tests first — everything red or characterized before any source edit

**Purpose**: Constitution III. After this phase every new test exists, the green-by-design ones are proven green, and the
red ones are shown red. **No file under `src/` changes in this phase**, apart from T020's scratch edit of `attributes.cpp`, which is reverted before the task ends.

**Story labels**: each test task is labelled with the story it checks.

### State-only test scaffolding

- [X] T007 [US1] In `tests/unit/state/CMakeLists.txt`, add the State-only scaffolding (research R11; precedent `tests/framebuffer/CMakeLists.txt`), **behind `option(OPENRENDER_STATE_ONLY_TESTS "Build the State-only isolation tests" OFF)`**. When the option is off, none of the following is defined at all (no `DependInfo.cmake` for archcheck to find):
  - a `STATIC` library `state_only_sources`, `EXCLUDE_FROM_ALL`, built from an explicit list of State sources:
    `src/ri/state/{attributes,options,pl,xform,displayChannel,variableLookup}.cpp` (later phases append `shaderParameterHooks.cpp` and `channelRegistry.cpp`);
  - it links `openrender_common_flags openrendercommon` and nothing else, `cxx_std_20`;
  - **include path, exactly**: `${CMAKE_SOURCE_DIR}/src`, `${CMAKE_SOURCE_DIR}/src/ri/state`, `${CMAKE_BINARY_DIR}` (for `config.h`), and a test-only staging
    directory `${CMAKE_CURRENT_BINARY_DIR}/state_only_include` that holds a `configure_file(... COPYONLY)` copy of `src/ri/core/ri_config.h`.
    `src/ri/core` itself must NOT be on the path (it holds the Shading shims `shader.h` and `shading.h`);
  - a helper `add_state_only_test(NAME EXE SOURCES…)` that adds an `EXCLUDE_FROM_ALL` executable linking `state_only_sources`, with the test
    `DISABLED TRUE`, `LABELS "state"`, `TIMEOUT 60`.
  - Comment: a bare `#include "shader.h"`, `"photonMap.h"`, `"dsply.h"` or `"blobbyField.h"` in State must fail to compile here; a domain-qualified spelling such as
    `"display/dsply.h"` still resolves through `src/` and is caught by the architecture check and the SC-003 grep, not by this test.
  - Comment the real reason for the option: archcheck's `loadMakefiles` walks every `DependInfo.cmake` (`EXCLUDE_FROM_ALL` targets included) and exits 2 "incomplete build tree" when an in-scope object is missing; the option is off until the library compiles (T050).
- [X] T008 [US1] Write `tests/unit/state/state_test_doubles.h` (header-only, includes `stateHandles.h` and `common/refCounter.h` only).
  - `CCountingShader : public CShaderGroupRefTarget` and `CCountingMap : public CMapRefTarget`, each with a static live-instance counter (constructor/destructor) and a
    helper `int refs(const CRefCounter &)` that reads the public `refCount`.
  - A fixture helper that mimics `CShaderInstance`'s constructor: `new CCountingShader` then `attach()` once (refCount 1), so `adopt()` can be tested against today's "constructor attaches, handler stores without attaching" behaviour (research R1).

### Tests (all [P]: different files)

- [X] T009 [P] [US1] Write `tests/unit/state/test_state_isolation.cpp` (`State_Isolation`): the handle and lifetime test. Every row of data-model.md "Ownership mapping" is asserted as a reference-count delta on a counting double:
  - **adopt** into each of `surface`, `displacement`, `atmosphere`, `interior`, `exterior` (`bindX(ShaderGroupRef::adopt(p))`): count unchanged by the bind;
  - **replace** a bound slot with another handle: the old object loses exactly one reference, and a destroyed object is freed;
  - **copy constructor** (`new CAttributes(src)`): each of the five slots and each of `globalMap`/`causticMap` gains exactly +1 (a copy that changes any count by anything other than +1 is the doubled-`attach` bug), and the clone's destruction gives exactly -1;
  - **`restore(src, shading=true, …)`**: the five slots are replaced (old -1, new +1); the two map handles are **untouched** (restore copies only the map names);
  - **light entries are non-owning**: `addLight`, copy, `restore`, `removeLight` and destruction never change a light double's count; `addLight` of the same light twice keeps one entry;
  - **`COptions::imager`**: adopt, copy-constructor +1, destruction -1;
  - **`TStateRef`**: empty handle is `false` and a bound one `true` (`explicit operator bool`); `adopt(nullptr)`/`retain(nullptr)` give an empty handle; copy +1; move leaves counts unchanged; assignment retains the new object before releasing the old one, and self-assignment is safe; `==`/`!=` against a handle and a raw pointer;
  - **motion chain**: a block with `next` set is copied recursively and each block keeps its own handles;
  - **no hook installed** (all of these tests run with no `RiBegin` session): after any `bindX`, `usedParameters == 0`, and `findParameter("x") == nullptr`;
  - `selectVolumeShader(&attr, solid, exterior)` returns the interior or exterior slot's `get()` (or `nullptr`);
  - Binding doubles that are not `CShaderInstance` is only legal because no hook is installed (contracts/shader-parameter-hooks.md "Precondition"): say so in a comment.
- [X] T010 [P] [US3] Write `tests/unit/state/test_state_last_bound.cpp` (`State_LastBound`): data-model.md's transition table, one assertion per row.
  - A fresh block: `governingMaterial() == EMaterialBinding::None`.
  - Surface then bxdf gives `Bxdf`; bxdf then surface gives `Surface`; **both handles stay bound** in either order.
  - `bindSurface(ShaderGroupRef())` and `bindBxdf(ShaderGroupRef())` (empty handles) **set the record** and leave the slot empty.
  - `setDefaultSurface(h)` stores the handle and leaves the record unchanged (FR-024): on a fresh block it stays `None`; over a bound bxdf it stays `Bxdf`.
  - Push/pop: a clone made with `new CAttributes(outer)`, `bindBxdf` on the clone over an outer surface, destroy the clone: the outer block still answers `Surface`, and the inner bxdf is gone with the scope.
  - The clone carries the source's answer; `restore(other, shading=true, …)` takes `other`'s value and also replaces the `bxdf` slot; `restore(…, shading=false)` leaves the record unchanged.
  - The `bxdf` slot's counts: adopt +0, copy +1, restore, destruction -1.
- [X] T011 [P] [US4] Write `tests/unit/state/test_state_pattern_table.cpp` (`State_PatternTable`): US4.1–4.4 and FR-013/014/015.
  - A node defined with `patterns.define("h", ref)` is found by `lookup("h")` in the same block and in a block cloned from it afterwards.
  - Define inside a clone, destroy the clone: the outer block does not find a handle that was defined only inside; an outer definition of the same handle is found again.
  - A new definition of an existing handle in the same scope **replaces** it in place (position kept); a nested definition shadows the outer one until the nested block is destroyed.
  - `lookup` of a never-defined handle returns `nullptr`, and an installed `CDiagnosticSink` receives **zero** messages (State writes no diagnostic, FR-015).
  - `forEach` visits entries in first-definition order with unique handles; `size()` matches.
  - Copy-on-write: after cloning, `define` in the clone does not change the source, and a later `define` in the source does not change the clone.
  - `restore(…, shading=true)` replaces the table with the other block's; `shading=false` leaves it.
  - **US4.4**: a node bound into a slot with `bindSurface(node)` stays alive (counting double still live, count as expected) after the defining block is destroyed.
- [X] T012 [P] [US5] Write `tests/unit/state/test_state_channel_registry.cpp` (`State_ChannelRegistry`), following contracts/channel-registry.md "Tests":
  - a fresh `CChannelRegistry`: `rgb` (3 samples, offset 0), `rgba` (4, 0), `a` (1, 3), `z` (1, 4), `rgbaz` (5, 0), in that declaration order via `forEach`; an unknown name gives `nullptr`;
  - `declare("float myaov")` through a **test-double declaration hook** (`installVariableDeclaration`, a captureless static function with the two-argument signature `CVariable *(const char *name, const char *declaration)`, like the core's adapter in T065) twice gives the same pointer;
  - the same name with a different shape gives `nullptr` and exactly one diagnostic `Channel "…" was previously defined differently` (code `CODE_SYSTEM`), captured with an installed `CDiagnosticSink`;
  - with **no** hook installed, a new string declaration gives `nullptr` and exactly one `Channel definition "…" is ill formed`;
  - `declare(CVariable *)`: the same variable returns the same channel; a different variable under an existing name returns `nullptr` with **no** message;
  - declare out of order across two simulated frames, then `resetUsage()`: every variable-backed channel's `sampleStart == -1` and the `forEach` order is unchanged (US5.3);
  - `beginChannelRegistry()` / `endChannelRegistry()` / `beginChannelRegistry()` gives a fresh instance with only the five built-ins; `channelRegistry()` is `nullptr` outside a session;
  - **session balance** (an exactly-once-deletion probe is not possible: the registry creates its `CDisplayChannel`s itself): run three `beginChannelRegistry()` / declare several channels / `endChannelRegistry()` cycles; each `begin` yields exactly the five built-ins, each `end` leaves `channelRegistry() == nullptr`, and the process completes without a crash or allocator error (a double delete would abort).
    A double delete need not abort in a Release build, so run this test once more with the allocator debug aids on macOS (`MallocScribble=1 MallocGuardEdges=1 ctest -R '^State_ChannelRegistry$' --output-on-failure`, through `cpujob`) and require it to pass there too.
- [X] T013 [P] [US1] Write `tests/unit/state/test_state_vocabulary.cpp` (`State_VocabularyConsistency`, modelled on `Common_VocabularyConsistency`). It is a Consumer, so it uses the normal include path
  (`src/`, `src/ri/state`, `src/display`, `src/ri/geometry`) and includes `stateVocabulary.h`, `dsply.h` and `blobbyField.h`.
  - Compare, at run time, all seven pairs and print the **first drifted pair by name**:
    `EDisplayParameterType::Float/Vector/Matrix/String/Integer == 0/1/2/3/4` against `FLOAT_PARAMETER`, `VECTOR_PARAMETER`, `MATRIX_PARAMETER`, `STRING_PARAMETER`, `INTEGER_PARAMETER`;
    `EBlobbyOrder::RiSpec = 0` and `EBlobbyOrder::AppNote = 1` against `BLOBBY_ORDER_RISPEC` and `BLOBBY_ORDER_APPNOTE`.
  - Comment the limitation from research R9: neither C enum has a count sentinel, so an enumerator *added* to `dsply.h` is not detected. `dsply.h` is frozen in 025.
- [X] T014 [P] [US2] Write `tests/unit/state/test_state_light_restore.cpp` (`State_LightOrderRestore`, State-only): the direct `restore()` and copy-constructor order check that the Ri-level `State_LightOrder` cannot isolate (its save/restore round trip reverses twice).
  - Build block A with light doubles L1, L2, L3 via `addLight` in that order; `new CAttributes(A)` must list L1, L2, L3; `restore` of A into a block B with a different list gives L1, L2, L3; a clone of a clone keeps the order; light switching (`removeLight` L2 then `addLight` L2) moves L2 to the end; no light's count changes anywhere.
  - Expected to be **red by assertion** until Phase 7 (the copy and restore prepend today), so it stays `DISABLED` until T071.
- [X] T015 [US2] Refactor the shared fixtures out of `tests/shading_parity/test_shader_parameter_equivalence.cpp` into a new header `tests/shading_parity/shader_parameter_fixtures.h` (`writeFile`, `compileRslo`, the four shader sources, `shaderBits`, `shaderLoads`, the oracle helpers `foundDefault`, `expectLookups`, `expectUsed`), as `static inline`.
  Part A keeps every case and every assertion. Do not change what it checks.
- [X] T016 [P] [US2] Write `tests/shading_parity/test_shader_parameter_equivalence_direct.cpp` (part B, `ShadingParity_ShaderParameterEquivalenceDirect`), using the same fixtures and the same binding sets as part A (none before `RiWorldBegin`, the world defaults, surface only, atmosphere over the default surface, all three, all three plus a light, the light switched off, displacement plus surface, the surface cleared, after `ResourceRestore`).
  - It asserts the same answers directly against `libshader::usedShaderParameters(const CAttributes &)` and `libshader::findShaderParameter(const CAttributes &, const char *)`, and that `libshader::shaderParameterHooks()` is `{ &usedShaderParameters, &findShaderParameter }`.
  - Includes `libshader/shading/shaderParameters.h`, which does not exist yet: it is **red by build failure** and stays `EXCLUDE_FROM_ALL` + `DISABLED` until T026.
  - Part A also checks, as it already does, that a fresh `RiBegin` block gets the hook (the "installation order" risk in plan.md).
- [X] T016a [P] [US2] Write `tests/shading_parity/test_shader_parameter_bxdf_ignored.cpp` (`ShadingParity_ShaderParameterBxdfIgnored`), using `shader_parameter_fixtures.h`. FR-005 / clarification 2026-10-26: binding a `bxdf` changes neither `libshader::usedShaderParameters` nor `libshader::findShaderParameter`.
  - Inside an `RiBegin` session, load the fixture shaders with `CRenderer::getShader`; for each binding set of part A (none, surface only, all three, all three plus a light) record both answers, then `bindBxdf(ShaderGroupRef::retain(shader))` with a real shader that declares a parameter name found nowhere else and bits no other slot sets, and assert both answers are **unchanged** (the bxdf's unique parameter is not found; the used-parameter set does not gain its bits).
  - Uses `CAttributes::bindBxdf`, which does not exist yet: it is **red by build failure**, `EXCLUDE_FROM_ALL` + `DISABLED` until T056.
- [X] T017 [P] [US3] Write `tests/unit/rib_parse/test_ribout_shader_order.cpp` (`RibOut_ShaderStatementOrder`, labels `rib_parse;unit`; spec 025 FR-023, US3.5), modelled on `test_sampling_option_roundtrip.cpp` (in-process, `riblex_destroy()` between sessions, libRIBOut writer).
  - Feed RIB text that holds `Surface`, `Displacement`, `Atmosphere`, `Interior`, `Exterior`, `LightSource` and `Imager` in at least three different orders, each interleaved with other attribute statements (`Color`, `Opacity`, `ShadingRate`, an `Attribute`).
  - Compare the written statements line by line with the input order.
  - **This is a characterization test: it is green on the starting tree and must stay green** (clarification Q3). Say so in the file header so nobody reads a green first run as a broken test. No `Bxdf` round-trip is added.
- [X] T018 Register the new tests (three CMake files):
  - `tests/unit/state/CMakeLists.txt`: `State_Isolation`, `State_LastBound`, `State_PatternTable`, `State_ChannelRegistry`, `State_LightOrderRestore` through `add_state_only_test` (all inside the `OPENRENDER_STATE_ONLY_TESTS` block of T007); `State_VocabularyConsistency` as a normal Consumer executable (`EXCLUDE_FROM_ALL`, `DISABLED`, normal include path, label `state`).
  - `tests/shading_parity/CMakeLists.txt`: the Direct test and the bxdf-ignored test (`EXCLUDE_FROM_ALL`, `DISABLED`, label `shading_parity`, same links and `ENVIRONMENT` as part A); part A's entry keeps working with the fixtures header.
  - `tests/unit/rib_parse/CMakeLists.txt`: `RibOut_ShaderStatementOrder`, a normal target (it builds now), links `ribRender openrendercommon`, same `ENVIRONMENT` as `SamplingOption_RibRoundTrip`.
  - The plan's single `State_Isolation` is split into `State_Isolation`, `State_LastBound`, `State_PatternTable`, `State_ChannelRegistry` and `State_LightOrderRestore`, so that each phase can turn its own test green. `ctest -L state` selects all of them.

### Red evidence

- [X] T019 [RUN] CP-a in `build-025` (option off: the State-only targets do not exist there). Then:
  - **Red evidence in a scratch build directory** (inside the worktree, deleted afterwards): configure it like `build-025` plus `-DOPENRENDER_STATE_ONLY_TESTS=ON`. Copy these cache variables from `build-025/CMakeCache.txt`: the generator (`CMAKE_GENERATOR`, Unix Makefiles), `CMAKE_BUILD_TYPE`, `CMAKE_TOOLCHAIN_FILE` and the `VCPKG_*` settings (`VCPKG_INSTALLED_DIR`, `VCPKG_TARGET_TRIPLET`, `VCPKG_OVERLAY_TRIPLETS`, `VCPKG_MANIFEST_*`), `BISON_EXECUTABLE`, `LLVM_DIR`, `CMAKE_OSX_DEPLOYMENT_TARGET`. Only `openrendercommon` and the State objects need building in the scratch tree, never the whole project. E.g. `cpujob 025-red-cfg cmake -B build-025-red -S . <build-025's cache options> -DOPENRENDER_STATE_ONLY_TESTS=ON`. In it run `cmake --build build-025-red --target state_only_sources` and `--target` for each of the five State-only executables. Each must **fail to compile**; record the error: `stateHandles.h` not found, and `shader.h`, `dsply.h`, `blobbyField.h` not found on the narrow path. Do not run archcheck there.
  - In `build-025`: `cmake --build build-025 --target <t>` for the vocabulary test, the Direct test and the bxdf-ignored test must each fail to compile (`stateVocabulary.h`, `shaderParameters.h`, `bindBxdf` missing). Record the errors.
  - `ctest -R 'ShadingParity_ShaderParameterEquivalence$'`: part A **64/64 green** after the T015 refactor.
  - `ctest -R 'RibOut_ShaderStatementOrder'`: green (the test is written by T017 and registered by T018, both before this task; it does not exist before Phase 2).
  - `ctest -R '^State_LightOrder$'`: red, 4 of 9 checks failing, as before.
  - `ctest -L 'state|shading_parity' --output-on-failure` in `build-025`: confirm the `DISABLED` tests report "Not Run (Disabled)" and that ctest's exit status does not count them as failures. If it does, stop and report instead of changing the convention.
  - Delete `build-025-red`. Record all of this in validation.md §2.
- [X] T020 [US2] [RUN] Re-prove part A's oracle after the refactor (quickstart §0.4, validation.md §0.4): in a scratch edit of `src/ri/state/attributes.cpp` (`findParameter`, around lines 427–452) search surface before displacement, rebuild, run `ShadingParity_ShaderParameterEquivalence`. Expected: **60 passed, 4 failed**. **Revert the edit** and confirm `git diff --stat -- src` is empty. Record the numbers.
- [X] T021 **Checkpoint 2 (TEST-APPROVAL, PAUSE 2)**: CP-a to CP-c (no source changed, so CP-d is not needed). Expected: all green except `State_LightOrder`; `ALLOWED DB-008 x 7`; archcheck passes with the option off (no State-only object is required). Stage the tests and CMake changes; summarize which tests are red, which are green by design, and why; wait for the maintainer's approval of the tests before Phase 3.

---

## Phase 3: Parameter hook — the logic moves to Shading (US1, US2; FR-003, FR-005, FR-006, FR-007)

**Goal**: `checkParameters()`'s union and `findParameter()`'s precedence live in Shading; State reaches them through a State-owned function-pointer hook (the `variableLookup.h` pattern). The slots are **still raw `CShaderInstance *`**. This phase comes first because, while the logic is in `attributes.cpp`, dropping `shader.h` from `attributes.h` would force a new non-allowlisted `attributes.cpp -> shader.h` edge.

**Independent test**: `ShadingParity_ShaderParameterEquivalence` (part A) and `…Direct` (part B) are green; every render is byte-identical.

- [X] T022 [US1] Create `src/ri/state/shaderParameterHooks.h` and `shaderParameterHooks.cpp` per contracts/shader-parameter-hooks.md:
  - `struct SShaderParameterHooks { unsigned int (*usedParameters)(const CAttributes &); CVariable *(*findParameter)(const CAttributes &, const char *name); };`
  - `void installShaderParameterHooks(const SShaderParameterHooks *hooks);` (`nullptr` uninstalls) and `const SShaderParameterHooks *shaderParameterHooks();`
  - Forward declarations of `CAttributes` and `CVariable` only; no other include beyond `common/`. Installed and removed on one thread, with the renderer's lifetime.
  - Add `state/shaderParameterHooks.cpp` to `ri_sources` in `src/ri/CMakeLists.txt` (ribVector inherits it from `ri_sources`; ribOut does not compile `attributes.cpp`, so it does not need it), and append it to `state_only_sources` in `tests/unit/state/CMakeLists.txt`.
- [X] T023 [US2] Create `src/libshader/shading/shaderParameters.h` and `shaderParameters.cpp` (namespace `libshader`), moving the logic **verbatim** from `src/ri/state/attributes.cpp:397-452`:
  - `unsigned int usedShaderParameters(const CAttributes &)`: the OR of `requiredParameters()` over displacement, surface, atmosphere, interior, exterior and every light, `| PARAMETER_N` when no surface is bound;
  - `CVariable *findShaderParameter(const CAttributes &, const char *name)`: the first match in displacement, then surface, then atmosphere; `nullptr` otherwise;
  - `const SShaderParameterHooks &shaderParameterHooks()` returning `{ &usedShaderParameters, &findShaderParameter }`;
  - **the `bxdf` slot is not part of either answer** (FR-005); add a comment naming spec 035.
  - In this phase the slots are still raw pointers, so the code reads `attrs.surface` etc. directly (T039 converts it to accessors). Add the file to `src/libshader/shading/CMakeLists.txt`.
- [X] T024 [US1] In `src/ri/state/attributes.h` and `attributes.cpp`, make `checkParameters()` store `hooks ? hooks->usedParameters(*this) : 0` and `findParameter(name)` return `hooks ? hooks->findParameter(*this, name) : nullptr`, and delete the old bodies. Keep both as members so `pl.cpp`, `blobby.cpp` and `rib.y` are **not edited**. `usedParameters` stays a stored field. The recompute points stay exactly where they are (constructor, `addLight`, `removeLight`, `restore(shading)`, and the core's calls).
- [X] T025 [US1] In `src/ri/render/rendererShadingServices.cpp` call `installShaderParameterHooks(&libshader::shaderParameterHooks())` from `rendererShadingServicesInit()`, and `installShaderParameterHooks(nullptr)` from `rendererShadingServicesShutdown()`.
  - **Not** from `rendererDeclarations.cpp`: ribVector compiles that file and does not link `libshader_shading`.
  - Confirm the order: `rendererShadingServicesInit()` runs in `beginRenderer()` right after `initDeclarations()`, before `CRendererContext` creates its first `CAttributes` (`rendererContext.cpp:109,139`).
- [X] T026 [US2] Enable part B: remove `EXCLUDE_FROM_ALL` and `DISABLED` from the Direct test in `tests/shading_parity/CMakeLists.txt`.
- [X] T027 [US2] [RUN] CP-a. Then run `ShadingParity_ShaderParameterEquivalence` and `…Direct` (both green, 64 checks each).
  - Mutation: in a scratch edit of `src/libshader/shading/shaderParameters.cpp` swap displacement/surface precedence, rebuild, and show **both** tests red (4 failures each). Revert and confirm `git diff` shows no residue.
  - Record in validation.md §3.
- [X] T028 [US1] [RUN] Guards that this phase must not move:
  - libshader_shading flat-namespace imports still **7** (validation.md §0.7 command: `nm -m <libshader_shading dylib> | grep 'dynamically looked up'`);
  - `ctest -L architecture` unchanged: `ALLOWED DB-008 x 7`, no new line (the hook adds no include or link edge, and the Shading file includes only State headers);
  - `ribVector` and `ribOut` still build and link (they are part of CP-a).
- [X] T029 **Checkpoint 3 (PAUSE 3)**: CP-a, CP-b, CP-c and **CP-d** (byte-identical output). `State_LightOrder` still 4 of 9. Record in validation.md; stage; summarize; wait for review.

---

## Phase 4: Handles, maps and the vocabulary (US1 and US2; FR-001 to FR-004, FR-008, FR-009)

**Goal**: Graphics State holds opaque handles; the 7 DB-008 include edges disappear. **No behaviour change.** The prepend loops in the copy constructor and `restore()` stay as they are (now over handle types); they change only in Phase 7.

**Independent test**: `ctest -L architecture` passes with no `DB-008` line; `State_Isolation` and `State_VocabularyConsistency` pass; the suite and every image are unchanged.

**Warning**: from T036 until T049 the tree does not compile (the slot fields become private). Do not build in between except to compile one object while fixing a site; the compiler is the work list (R12).

- [X] T030 [US1] Delete the 7 DB-008 lines and their section header (`# --- DB-008: …`) from `tests/architecture/allowlist.txt` (lines 135–142). Add and swap nothing. [RUN] `ctest -L architecture` (the option is still off, so the State-only library does not exist and cannot add a duplicate edge): expected **red, exactly 7 `VIOLATION` lines, all with source under `src/ri/state`** (T-A1). Any other violation means the design is wrong: stop. Record the 7 lines in validation.md §4.
- [X] T031 [US1] Create `src/ri/state/stateHandles.h` (header-only; includes `common/refCounter.h` only) per contracts/state-handles.md:
  - `class CShaderGroupRefTarget : public CRefCounter` and `class CMapRefTarget : public CRefCounter`, each with no data members and a `protected` defaulted constructor;
  - `template <class T> class TStateRef` with `adopt(T*)` (takes over the caller's reference, no count), `retain(T*)` (attaches a non-null object), copy (+1), move (no count change), copy assignment (retain the new object, then release the old), move assignment, destructor (`detach()`), `reset()`, `get()`, `explicit operator bool`, and `==`/`!=` against a handle and a raw `T*`;
  - the invariant "a non-empty handle owns exactly one reference to `object`"; "Empty means no object"; a null `T*` passed to `adopt`/`retain` gives an empty handle;
  - `using ShaderGroupRef = TStateRef<CShaderGroupRefTarget>;` and `using MapRef = TStateRef<CMapRefTarget>;` — distinct types that do not convert to each other;
  - every operation inline, so no out-of-line State symbol is added for Shading or Hiders to import.
- [X] T032 [P] [US1] Create `src/ri/state/stateVocabulary.h` (contracts/state-vocabulary.md): `enum class EDisplayParameterType : int { Float = 0, Vector = 1, Matrix = 2, String = 3, Integer = 4 };` and `enum class EBlobbyOrder : int { RiSpec = 0, AppNote = 1 };`. No include of `dsply.h` or `blobbyField.h`.
- [X] T033 [US1] In `src/libshader/shading/shader.h` change `CShaderInstance`'s base from `CRefCounter` to `public CShaderGroupRefTarget` and include `stateHandles.h` (Shading → State, allowed). The new base has no data members, so the layout is unchanged. Confirm `CProgrammableShaderInstance`, `CSphereLight` and `CQuadLight` (`src/ri/hcshader.h`) still reach it through single inheritance with the reference-counted base first; the shim `src/ri/core/shader.h` needs no change.
- [X] T034 [P] [US1] Create `src/libshader/shading/shaderRefs.h` (inline only), per contracts/state-handles.md:
  - `shaderInstanceOf(const CShaderGroupRefTarget *)` and `shaderInstanceOf(const ShaderGroupRef &)`, each a `static_cast` (const_cast on the argument);
  - `lightInstanceOf(const CActiveLight &)` returning `static_cast<CProgrammableShaderInstance *>(shaderInstanceOf(e.light))`: **unchecked, as before** — add the comment naming DB-013 (the built-in sphere and quad lights are not programmable instances; the consumers read only `CShaderInstance` members). No `reinterpret_cast`, `void *`, `dynamic_cast` or assert.
- [X] T035 [P] [US1] In `src/ri/hiders/photonMap.h` change `CPhotonMap`'s base list (line ~74) so `public CRefCounter` becomes `public CMapRefTarget`; include `stateHandles.h` (Hiders → State, allowed); add the inline `CPhotonMap *photonMapOf(const MapRef &r) { return static_cast<CPhotonMap *>(r.get()); }`.
- [X] T036 [US1] `src/ri/state/attributes.h`:
  - remove `#include "shader.h"` and the `class CPhotonMap;` forward declaration; include `stateHandles.h`;
  - `CActiveLight::light` becomes `CShaderGroupRefTarget *` (non-owning); `addLight`/`removeLight` take `CShaderGroupRefTarget *`;
  - the five slots become **private** `ShaderGroupRef` members read through inline `const ShaderGroupRef &surface() const` (and `displacement()`, `atmosphere()`, `interior()`, `exterior()`) and written only through `bindSurface`, `bindDisplacement`, `bindAtmosphere`, `bindInterior`, `bindExterior` (each stores and calls `checkParameters()`) and `setDefaultSurface` (stores and recomputes; used by `RiWorldBegin` only);
  - `globalMap`, `causticMap` become public `MapRef`;
  - `selectVolumeShader(...)` returns `CShaderGroupRefTarget *`.
- [X] T037 [US1] `src/ri/state/attributes.cpp`:
  - remove `#include "photonMap.h"` and any shader include;
  - **delete** the copy constructor's 5 slot and 2 map `attach()` lines and the destructor's matching `detach()` lines (the handle members do this now; keeping them would double every count);
  - `restore(shading)` assigns the five slots (the old `detach`/`attach` pairs go) and still copies map *names* only, never the map handles;
  - keep the two **prepend** loops over `lightSources` exactly as they are (Phase 7 changes them);
  - `addLight` stores the `CShaderGroupRefTarget *` with **no cast** (the cast is now in `lightInstanceOf`), still appends and suppresses duplicates; `removeLight` unchanged.
- [X] T038 [US1] `src/ri/state/options.h` and `options.cpp`: remove `#include "shader.h"`, `"dsply.h"` and `"blobbyField.h"`; include `stateHandles.h` and `stateVocabulary.h`.
  - `imager` becomes a public `ShaderGroupRef`; delete its manual `attach`/`detach` in the copy constructor and destructor (`options.cpp` ~454 and ~519);
  - `COptions::CDisplay::TDisplayParameter::type` becomes `EDisplayParameterType`, and the copy constructor's `switch` (~136–148) uses the State names;
  - `blobbyOpcodeOrder` stays `int` with the default `static_cast<int>(EBlobbyOrder::RiSpec)`.
- [X] T039 [US2] Convert `src/libshader/shading/shaderParameters.cpp` to read the slots through the accessors and `shaderInstanceOf`/`lightInstanceOf` (the logic is unchanged; the `bxdf` slot is still absent).
- [X] T040 [US2] `src/libshader/shading/shading.cpp`: include `shaderRefs.h` and read every slot and light through `shaderInstanceOf(currentAttributes->X())` / `lightInstanceOf(*cLight)`. The sites are:
  - ~865–921 and ~975 (shader selection per grid), ~1011–1014 (`prepare`);
  - ~1271 (`selectVolumeShader` result), ~1556–1613 (`getParameter` of surface/displacement/atmosphere/interior/exterior);
  - ~1907, ~1970, ~2200 (light loops);
  - ~2753–2770 (`getName`), ~4172 and ~4183 (`postShader` from interior/exterior).
  The hot loops must not touch a reference count: slot accessors return `const &`, and `shaderInstanceOf` is an inline `static_cast`.
- [X] T041 [P] [US2] `src/libshader/shading/executeMisc.cpp:846-847` (interior/exterior) and `src/libshader/shading/execute.cpp:409` (the `illuminance` light loop macro): same conversion with `shaderInstanceOf`/`lightInstanceOf`.
- [X] T042 [P] [US1] Hiders read the photon maps through `photonMapOf(attributes->globalMap)` / `photonMapOf(attributes->causticMap)`: `src/ri/hiders/photon.cpp:628-660`, `src/ri/hiders/radiance.cpp:315-321` and `:1119-1134`, `src/ri/hiders/irradianceDispatch.cpp:477-487`. Include `photonMap.h` (Hiders may). Same pointer, same null checks.
- [X] T043 [P] [US1] `src/ri/geometry/surface.cpp:605`: the presence check becomes `attributes->displacement()` (an `explicit operator bool` in an `if`/`&&`). Geometry reads State only; it must not include `shaderRefs.h` or `photonMap.h`.
- [X] T044 [US1] `src/ri/render/rendererContext.cpp` shader binders: replace the `detach(); slot = cShader;` pairs and the separate `checkParameters()` calls with the setters.
  - `RiSurfaceV`, `RiAtmosphereV`, `RiInteriorV`, `RiExteriorV` (~2354–2409) and `RiDisplacementV` (~3302): `attributes->bindX(ShaderGroupRef::adopt(cShader))`. The setter recomputes, so the old `checkParameters()` calls (lines ~2358, 2375, 2392, 2409, 3306) go, one-for-one.
  - `RiWorldBegin` (~990–997): `if (!currentAttributes->surface())`, then `attrs->setDefaultSurface(ShaderGroupRef::adopt(cShader))`; the `checkParameters()` at ~995 goes.
  - `RiImagerV` (~1292): `options->imager = ShaderGroupRef::adopt(s)`; delete the manual `detach`.
  - `getShader()` returning `NULL` for `"null"` or a load failure still leaves the slot empty (an empty handle).
  - `RiLightSourceV`, `RiAreaLightSourceV`, `RiIlluminate` and the default-light binding need **no edit**: they pass a `CShaderInstance *` to `addLight`/`removeLight`, which now take its State base `CShaderGroupRefTarget *` (an implicit upcast).
- [X] T045 [US1] Display-parameter and blobby-order conversions in the renderer core.
  - Create `src/ri/render/stateConversions.h` (core-private; location decided in spec clarification 2026-10-26): one inline helper converting the plug-in's `ParameterType` to `EDisplayParameterType` (and back), with a `static_assert` per enumerator pair beside it.
  - `src/ri/render/rendererContext.cpp:1417-1448`: the five writes use `EDisplayParameterType::…`. The `BLOBBY_ORDER_*` assignments at ~1978–1985 stay `int` assignments (add `#include "dsply.h"` where needed; `blobbyField.h` is already included at line 36).
  - `src/ri/render/rendererDisplay.cpp:48-57`: the comparison of `findParameter`'s `ParameterType` against the stored type goes through the helper (mixed-enum comparison is an error under `-Werror`). Add `#include "dsply.h"`.
- [X] T046 [P] [US1] `src/ri/render/renderer.cpp`: line ~279 `imagerShader = shaderInstanceOf(o->imager)`; lines ~953–965 the lazy map binding becomes `cAttributes->globalMap = MapRef::retain(getPhotonMap(name))` (and the same for `causticMap`), keeping the `if (… == NULL)` laziness so two objects sharing a name still get the same map; add `photonMap.h` and `shaderRefs.h` includes.
- [X] T047 [P] [US1] `src/ri/render/geometryDispatch.cpp:281`: `attributes->displacement() && (attributes->flags & ATTRIBUTES_FLAGS_DISPLACEMENTS)`.
- [X] T048 [P] [US2] Adapt the consumers; **assertions do not change**, only how the object is reached:
  - `tests/unit/csg/test_interior_exterior_selection.cpp`: replace the `0x1`/`0x2` sentinel pointers with counting doubles (a retaining handle would dereference a sentinel) bound with `bindInterior`/`bindExterior`, compare with `selectVolumeShader(...)` against the doubles' `get()`, and `leaf->attributes->exterior().get()`. **Bind the doubles with no `RiBegin` session open** (contracts/shader-parameter-hooks.md "Precondition"): the double-binding cases already run before the CSG case that opens a session; keep that order, or give them their own process. The doubles are bound only in blocks that never see `restore()` or a shading call, which would reach the hook;
  - `tests/imager/test_imager_guard.cpp` (~70, ~80) and `tests/imager/test_imager_options.cpp` (~63–124): `opts->imager` is a handle; use `.get()`/`shaderInstanceOf`, keep every `EXPECT_*`;
  - `tests/shading_parity/test_ambient_accumulation.cpp` and `tests/unit/state/test_light_order.cpp`: adjust only if the compiler requires it.
- [X] T049 [US1] [RUN] Compiler-guided sweep (research R12): `cmake --build build-025 --config Release -j4` until clean, including `hcshader.{h,cpp}`, `rendererContext.h`, `renderer.h`, the preview, Python and Lua consumers if they are in this build.
  - A unit that lost a transitive include is fixed **only** with an infrastructure header (`common/…`, `ri_config.h`), its own domain's header, or — in the renderer core and in consumers — any header.
  - **STOP and report** if a Geometry, Texturing or Scene Interface unit would need a Shading, Hiders or Display header. Never work around it with a forbidden include.
  - Record the list of added includes in validation.md §4.
- [X] T050 [US1] Enable the State-only tests.
  - In `tests/unit/state/CMakeLists.txt` change the option's default to **ON**, and in `build-025` reconfigure with `-DOPENRENDER_STATE_ONLY_TESTS=ON` (the cached value is OFF and a changed default does not override a cache entry).
  - Remove `EXCLUDE_FROM_ALL` from `state_only_sources` (so it is part of `all` and archcheck finds its objects) and from `State_Isolation`'s target, and remove `State_Isolation`'s `DISABLED` marker; do the same for `State_VocabularyConsistency` (its headers exist now). Keep `State_LastBound`, `State_PatternTable`, `State_ChannelRegistry` and `State_LightOrderRestore` `EXCLUDE_FROM_ALL` + `DISABLED` until their phases (their own compile-only dependencies are test sources, not `src/` objects).
  - [RUN] build, then `ctest -R 'State_Isolation|State_VocabularyConsistency' --output-on-failure`: both green. Record that T019's compile errors are gone.
  - **Mutation (C1)**: in a scratch edit make the copy constructor retain one slot twice (or keep one of the deleted manual `attach()` lines), rebuild, show `State_Isolation` **red** on the count check; revert and confirm `git diff` clean for that file.
- [X] T051 [US1] [RUN] Mutation (SC-007): in a scratch edit of `src/ri/state/stateVocabulary.h` change one value (e.g. `EBlobbyOrder::AppNote = 2`), rebuild, show `State_VocabularyConsistency` **red and naming the drifted pair**; revert, confirm `git diff` clean for that file.
- [X] T052 [US1] Greps and diffs (quickstart §3), results into validation.md §4:
  - SC-003: `grep -nE '#include *"(.*/)?(shader|photonMap|dsply|blobbyField|shading)\.h"' src/ri/state/*` returns nothing, directly or through a shim;
  - `git diff --stat dfffba0 -- src/display/dsply.h src/ri/geometry/blobbyField.h` is empty (FR-008);
  - `git diff --stat dfffba0 -- src/ri/parse/rib.y src/ri/output src/ri/texture/texture3d.cpp src/ri/geometry/blobby.cpp src/ri/state/pl.cpp` is empty (callers not edited).
- [X] T053 [US1] [RUN] Architecture and import checks:
  - `ctest -L architecture`: **PASS with no `DB-008` in `ALLOWED`**, no `STALE`, no `VIOLATION`, no Graphics State source in any edge (SC-001). Delete any line reported `STALE` (deletions only); lower `tests/architecture/metrics-baseline.txt` only if `NOTE metric … < baseline` is printed.
  - **Research R11**: this is the first run with the State-only library in the build tree (option on since T050). It had to wait: archcheck's `loadMakefiles` walks every `DependInfo.cmake`, `EXCLUDE_FROM_ALL` targets included, and exits 2 "incomplete build tree" when an in-scope object is missing, so the library could not exist unbuilt earlier. The State sources compiled a second time must only repeat the production edges, not double-count them in a way that breaks the counts, and the staged `ri_config.h` copy must not be reported as unclassified. If either fails, **stop and report**.
  - libshader_shading flat-namespace imports still **7**.
- [X] T054 **Checkpoint 4 (PAUSE 4)**: CP-a, CP-b, CP-c, **CP-d**. Name these explicitly in the report: `photonmap_*` frame-release and session tests, the imager tests, `CSG_InteriorExteriorSelection`, `State_Isolation`, `State_VocabularyConsistency`, `ShadingParity_ShaderParameterEquivalence{,Direct}`. `State_LightOrder` still 4 of 9. Stage; summarize (including the list of added includes); wait for review.

---

## Phase 5: The last-bound record and the pattern table (US3, US4; FR-010 to FR-015, FR-024)

**Goal**: the `bxdf` slot, the Surface/Bxdf record (D5) and the scoped pattern-node table exist and are exercised through the state API. No RIB statement is added (FR-012). No behaviour change.

### User Story 3 — which of Surface and Bxdf was bound last (P2)

**Independent test**: `State_LastBound` green; `RibOut_ShaderStatementOrder` still green; renders unchanged.

- [X] T055 [US3] `src/ri/state/attributes.h` / `attributes.cpp`:
  - add `enum class EMaterialBinding : unsigned char { None, Surface, Bxdf };` and the field `lastMaterialBinding`, starting `None`;
  - add the private `ShaderGroupRef bxdf` with the inline `const ShaderGroupRef &bxdf() const`, and `void bindBxdf(ShaderGroupRef)` (stores, sets the record to `Bxdf` **even when the handle is empty**, recomputes);
  - `bindSurface` also sets the record to `Surface` **even when the handle is empty** (today's `Surface "null"` or failed-load behaviour, plus the record);
  - `bindDisplacement/Atmosphere/Interior/Exterior` leave the record unchanged; **`setDefaultSurface` leaves it unchanged (FR-024)**;
  - `EMaterialBinding governingMaterial() const` is the single query (FR-011); no consumer may infer the answer from which slot is empty;
  - the copy constructor carries the record by value (memberwise) and the `bxdf` handle with a retain; `restore(shading=true)` assigns the `bxdf` slot and copies the record explicitly; `restore(…, shading=false)` leaves both unchanged;
  - the `bxdf` slot stays out of `usedShaderParameters` and `findShaderParameter` (FR-005, comment already in place from T023).
- [X] T056 [US3] Enable `State_LastBound` (remove `EXCLUDE_FROM_ALL` and `DISABLED`) **and `ShadingParity_ShaderParameterBxdfIgnored`** (same two markers); [RUN] build and `ctest -R 'State_LastBound|ShadingParity_ShaderParameterBxdfIgnored' --output-on-failure`: both green.
  - **Mutation (C1)**: in a scratch edit make `setDefaultSurface` set the record to `Surface`; rebuild; `State_LastBound` goes **red** on the FR-024 check. Revert and confirm `git diff` clean. Second scratch edit: include the `bxdf` slot in `libshader::usedShaderParameters`; `ShadingParity_ShaderParameterBxdfIgnored` goes red. Revert.
- [X] T057 [US2] [RUN] Confirm `ShadingParity_ShaderParameterBxdfIgnored` is green (the bxdf slot is ignored by both answers until spec 035) together with `…EquivalenceDirect`; record in validation.md §5. No new test code in this task.
- [X] T058 [US3] Confirm FR-012 and FR-023: `git diff --stat dfffba0 -- src/ri/parse src/ri/output` is empty (no grammar, no `Bxdf` or `Pattern` statement, ribOut unchanged), and `RibOut_ShaderStatementOrder` still passes.

### User Story 4 — a scoped pattern-node table (P2)

**Independent test**: `State_PatternTable` green.

- [X] T059 [US4] Create `src/ri/state/patternTable.h` (header-only), `CPatternTable`, per data-model.md and contracts/attribute-scope.md:
  - storage: a reference-counted, copy-on-write block holding a `std::vector` of entries `{ std::string handle; ShaderGroupRef node; }`; "Entries are kept in first-definition order, with unique handles";
  - `void define(const char *handle, ShaderGroupRef node)`: if the storage is shared, copy it first; then replace the entry with the same handle **in place** (position kept) or append;
  - `const ShaderGroupRef *lookup(const char *handle) const`: `nullptr` when the handle is not found; **State writes no diagnostic** (FR-015);
  - `template <class F> void forEach(F &&) const` in definition order; `int size() const`;
    - the callback shape the approved `State_PatternTable` uses is `fn(handle, node)`: `handle` converts to `const std::string &` and `node` to `const ShaderGroupRef &`;
  - copying the table shares the storage (O(1)); State never interprets a node's content (FR-015). Document that attribute blocks are modified only on the single RIB-stream thread, which the copy-on-write check relies on.
- [X] T060 [US4] `src/ri/state/attributes.h` / `attributes.cpp`: add the member `CPatternTable patterns;`. The copy constructor shares it through the memberwise assignment; `restore(shading=true)` assigns it; `restore(…, shading=false)` leaves it. Scoping needs nothing more: `AttributeBegin` clones the block, so an inner definition lives in the inner copy (value semantics, research R7). Nothing in 025 consumes the table.
- [X] T061 [US4] Enable `State_PatternTable` (remove `EXCLUDE_FROM_ALL` and `DISABLED`); [RUN] build and `ctest -R State_PatternTable --output-on-failure`: green.
  - **Mutation (C1)**: in a scratch edit make `define` append instead of replacing an existing handle (or skip the copy-on-write copy); rebuild; `State_PatternTable` goes **red**. Revert and confirm `git diff` clean.
- [X] T062 **Checkpoint 5 (PAUSE 5)**: CP-a, CP-b, CP-c, **CP-d**. `State_LastBound`, `State_PatternTable`, `…EquivalenceDirect` and `RibOut_ShaderStatementOrder` green; `State_LightOrder` still 4 of 9; the architecture report has no new line. Stage; summarize; wait for review.

---

## Phase 6: The channel registry moves to Graphics State (US5; FR-016 to FR-018)

**Goal**: no channel table among `CRenderer`'s statics; `CRenderer::retrieveDisplayChannel` remains as an **out-of-line** forwarder; `texture3d.cpp` has no diff; DB-021 stays Open.

**Independent test**: `State_ChannelRegistry` green; the AOV visual tests and `Parity_transparency-matte-aov` unchanged.

- [X] T063 [US5] `src/ri/state/variableLookup.h` / `variableLookup.cpp`: add the second hook beside the lookup:
  - `using TVariableDeclaration = CVariable *(*)(const char *name, const char *declaration);`
  - `void installVariableDeclaration(TVariableDeclaration);` (`nullptr` uninstalls) and `CVariable *declareVariableThroughHook(const char *name, const char *declaration);` (returns `nullptr` when no hook is installed).
- [X] T064 [US5] Create `src/ri/state/channelRegistry.h` and `channelRegistry.cpp` per contracts/channel-registry.md:
  - `class CChannelRegistry` with fields `CTrie<CDisplayChannel*> byName` and `CArray<CDisplayChannel*> inOrder`; "The registry owns each channel exactly once";
  - the constructor registers, in this order: `rgb` (3 samples, offset 0), `rgba` (4, 0), `a` (1, 3), `z` (1, 4), `rgbaz` (5, 0);
  - `declare(const char *)` is today's `CRenderer::declareDisplayChannel(const char *)` (`rendererDeclarations.cpp:534-570`) moved unchanged: `parseVariable` failure returns `nullptr` with no message; an existing name returns it when `numSamples` matches and either the declaration is not global storage or `outType` matches, otherwise `error(CODE_SYSTEM, "Channel \"%s\" was previously defined differently\n", name)`; a new name goes through `declareVariableThroughHook(NULL, declaration)`, and `nullptr` (including when no hook is installed) gives `error(CODE_SYSTEM, "Channel definition \"%s\" is ill formed\n", declaration)`; otherwise create `CDisplayChannel(var->name, var, var->numFloats, -1, global ? var->entry : -1)`. **Error strings, codes, order and ownership stay byte-identical**;
  - `declare(CVariable *)` is today's overload (`:579-600`): an existing name returns the channel when its variable is the same one, else `nullptr` with no message;
  - `retrieve(const char *) const`, `resetUsage()` (`sampleStart = -1` for variable-backed channels, in declaration order), `forEach(F &&)`;
    - the callback shape the approved `State_ChannelRegistry` uses is `fn(channel)` with `channel` converting to `const CDisplayChannel *`; `retrieve()` returns a non-const `CDisplayChannel *` (the test sets `sampleStart` through it);
  - the destructor deletes each channel once and frees both containers (today's `destroy()` plus `delete`);
  - `beginChannelRegistry()`, `endChannelRegistry()`, `channelRegistry()` (`nullptr` outside a session);
  - **no include of any other domain's header**. Add the file to `ri_sources` **and to `ribOut_sources`** in `src/ri/CMakeLists.txt` (ribOut compiles `rendererDeclarations.cpp`, which will call `beginChannelRegistry()`; plan.md's source list omits this), and append it to `state_only_sources` in `tests/unit/state/CMakeLists.txt`.
- [X] T065 [US5] `src/ri/render/rendererDeclarations.cpp`:
  - `initDeclarations()` (~286–304 and the registrations): replace the two `new`s and the five built-in registrations with `beginChannelRegistry()`, and install a small adapter with `installVariableDeclaration`, next to the existing `installVariableLookup` call. `&CRenderer::declareVariable` does **not** convert to the hook type, because `declareVariable(const char *, const char *, int um = 0)` takes a third parameter. Define a captureless static function in this file, `static CVariable *declareVariableAdapter(const char *name, const char *declaration) { return CRenderer::declareVariable(name, declaration, 0); }`, and install `&declareVariableAdapter`;
  - `shutdownDeclarations()` (~352–355): uninstall the declaration hook at the top beside `installVariableLookup(nullptr)`, and call `endChannelRegistry()` at **today's position**, before the declared variables are destroyed;
  - delete `declareDisplayChannel` (both overloads) and `resetDisplayChannelUsage` (~534–633);
  - define `CDisplayChannel *CRenderer::retrieveDisplayChannel(const char *name)` **out of line** here as `{ return channelRegistry() ? channelRegistry()->retrieve(name) : nullptr; }`. It must not be inline: an inline forwarder would make `texture3d.o` reference State symbols, which would make DB-010's link line STALE and add a Texturing → State VIOLATION.
- [X] T066 [US5] `src/ri/render/renderer.h` and `src/ri/render/rendererStatics.cpp`: delete `declaredChannels`, `displayChannels` (renderer.h:151-152; statics.cpp:118-119), both `declareDisplayChannel` declarations and `resetDisplayChannelUsage` (renderer.h:270-273). **Keep** the `retrieveDisplayChannel` declaration.
- [X] T067 [US5] Callers use the State registry directly (the core may depend on State): `src/ri/render/rendererContext.cpp:1483` (`RiDisplayChannelV`) → `channelRegistry()->declare(channel)`; `src/ri/render/rendererDisplay.cpp:563` → `channelRegistry()->resetUsage()`, `:618` → `channelRegistry()->retrieve(sampleName)`, `:671` → `channelRegistry()->declare(cVar)`. Include `channelRegistry.h`. Behaviour (including a failed declaration) is unchanged.
- [X] T068 [US5] Enable `State_ChannelRegistry` (remove `EXCLUDE_FROM_ALL` and `DISABLED`); [RUN] build (all targets, including `ribOut` and `ribVector`) and `ctest -R State_ChannelRegistry --output-on-failure`: green.
  - **Mutation (C1)**: in a scratch edit make `resetUsage()` skip the last channel (or reorder the built-ins); rebuild; `State_ChannelRegistry` goes **red**. Revert and confirm `git diff` clean.
- [X] T069 [US5] Greps and object check (quickstart §3, US5.4/US5.5, SC-008), results into validation.md §6:
  - `grep -nE 'declaredChannels|displayChannels|declareDisplayChannel|resetDisplayChannelUsage' src/ri/render/*` returns nothing;
  - `retrieveDisplayChannel` appears only in `renderer.h` (declaration) and `rendererDeclarations.cpp` (out-of-line definition);
  - `git diff --stat dfffba0 -- src/ri/texture/texture3d.cpp` is empty;
  - `nm` on `texture3d.o` shows an undefined `CRenderer::retrieveDisplayChannel` and **no** `channelRegistry`/`CChannelRegistry` symbol.
- [X] T070 **Checkpoint 6 (PAUSE 6)**: CP-a, CP-b, CP-c, **CP-d**. Name explicitly: every AOV visual test, `Parity_transparency-matte-aov`, `State_ChannelRegistry`. The architecture report keeps DB-010's link line and DB-021's include line for `texture3d.cpp`, with no `STALE` and no new line. `State_LightOrder` still 4 of 9. Stage; summarize; wait for review.

---

## Phase 7: Light order — the ONLY phase allowed to change output (US2; FR-025, SC-010)

**Goal**: lights run in RIB order in every block. An attribute push, a copy-on-write clone and a `ResourceRestore` copy the active-light list in order instead of reversing it. The plan-stage scan predicts **82 affected scenes, no changed 8-bit image, no changed `MaxBlockAvgDiff`** (validation.md §0.5); this phase confirms it.

**Independent test**: `State_LightOrder` 9/9 and `State_LightOrderRestore` pass; the 66 affected visual and parity tests give identical diffs and bit-identical images; the determinism set and sweep pass.

- [X] T071 [US2] Enable `State_LightOrderRestore` (remove `EXCLUDE_FROM_ALL` and `DISABLED`). [RUN] build and run `State_LightOrder` and `State_LightOrderRestore` on the tree **before** the fix: both **red by assertion** (`State_LightOrder` 4 of 9). Record the failing checks in validation.md §7. This is the red evidence for FR-025.
- [X] T072 [US2] `src/ri/state/attributes.cpp`: add the file-local `static CActiveLight *copyLightList(const CActiveLight *source)`, which returns a new list holding the same (non-owning) entries **in the source's order**, built with a tail pointer: O(n), no reversal. Use it in both the copy constructor and `restore(shading)` (which first frees its own entries), replacing the two prepend loops. `addLight` (append, duplicate suppression) and `removeLight` (unlink the first match) do not change. Fix the stale comments. Compare with `tools/lightorder-fix-sketch.diff`.
- [X] T073 [US2] [RUN] Build, then `State_LightOrder` (**9/9**) and `State_LightOrderRestore` (green) pass. Record. (Their red evidence in T071 is the mutation proof; no further scratch edit.)
- [X] T074 [US2] [RUN] The affected tests: `ctest -R "$(cat specs/025-graphics-state-handles/tools/affected-regex.txt)" -j4 -V` (log to `build-025/logs/025-p7-affected.log`).
  - Write `specs/025-graphics-state-handles/tools/affected_diff_check.sh`, which extracts every `MaxBlockAvgDiff` from the log and compares it with the starting-tree column of `tools/affected-diff-compare.tsv`.
  - Expected: 66/66 pass at **unchanged thresholds**; all 63 printed diffs **identical**.
- [X] T075 [US2] [RUN] Images: `image_manifest.sh diff tools/baseline-images.sha256 build-025 tools/baseline-image-noise.txt` after a clean full-suite run (CP-b): expected 0 differing. The 95 images of `tools/affected-bitwise.tsv` are the ones the plan predicts as bit-identical.
- [X] T076 [US2] [RUN] Determinism and the sweep:
  - `cpujob 025-p7-det ctest --test-dir build-025 -L '^determinism$' -j2 --output-on-failure`;
  - the full suite (CP-b);
  - the opt-in sweep, once: `cpujob 025-p7-sweep ctest --test-dir build-025 -L '^determinism-sweep$' -j2 --output-on-failure` (about 10–12 minutes);
  - `DepthDeterminism_*` and `ColorDeterminism_*` stay bit-identical.
  - The probe did not observe light order inside procedurals expanded at render time (`csg-procedural-open-cylinder{,-inline}-raytrace`, `csg-procedural-missing-archive`); the full suite covers them. Record that they passed.
- [X] T077 [US2] **Decision task — STOP condition.** Evaluate T074–T076 and write validation.md §7:
  - If every diff, image and test result equals the baseline: write "no reference changed" and continue.
  - **If any reference result changed** (a different `MaxBlockAvgDiff`, a different image, a red test at its unchanged threshold): do **not** regenerate a reference, do **not** touch a threshold. List each affected scene with its old and new diff and the reason (light order), **stop, and ask the maintainer** for approval. A reference may be re-rendered only with the maintainer's explicit approval, given at that time, for a listed scene and reason (FR-025).
- [X] T078 **Checkpoint 7 (PAUSE 7)**: CP-a, CP-b, CP-c. **Everything is green, with no allowed exception** (`State_LightOrder` is now 9/9). CP-d is replaced by T075. Stage; summarize with the T077 outcome; wait for review.

---

## Phase 8: Registry and documentation, then Linux (no story label)

**Purpose**: the guardrails, the docs and the validation package. Do not refer to the merge in any text.

- [ ] T079 [P] Update `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` (FR-020, research R13). `<date>` is the date this task is done.
  - **DB-008**: move it from `## Open` to `## Resolved` as "Resolved by spec 025 (`specs/025-graphics-state-handles/`), <date>". Summarize: 7 include edges to 0; `ShaderGroupRef`/`MapRef` handles with a `bxdf` slot; the used-parameter logic in Shading behind a State-owned hook; the state vocabulary header; the channel registry in State; light order. Its tracking line names #31 only.
  - **DB-021**: stays **Open**. Evidence: drop "the registry itself is renderer-core state"; keep Texturing's include of `displayChannel.h` and its call through the `CRenderer::retrieveDisplayChannel` forwarder, which now forwards to the State-owned registry. Tracking: spec 037 ([#26]). Direction: the registry is owned by Graphics State; passed to consumers as channel descriptors (037). No text may call DB-021 Resolved.
  - **DB-010**: the evidence notes that the forwarder now forwards to the State-owned registry; its edge count stays 35.
  - **DB-013**: add a History bullet (the entry has none yet; use DB-001's History format): the unchecked `CShaderInstance *` to `CProgrammableShaderInstance *` cast now lives in `lightInstanceOf()` (`src/libshader/shading/shaderRefs.h`), preserved as-is.
  - **DB-018**: if its evidence quotes `attributes->surface`-style reads, update the wording to `shaderInstanceOf(attributes->surface())`; it is touched, not extended.
  - Update any count in the file's Architecture-check section that quotes the allowlist total.
- [ ] T080 [P] Allowlist and metric: `git diff dfffba0 -- tests/architecture/allowlist.txt | grep -c '^+[A-Z]'` is **0**; the header counts agree with the archcheck report (SC-009); `tests/architecture/metrics-baseline.txt` is unchanged unless the check printed `NOTE metric … < baseline`, in which case lower it to exactly the reported value and never raise any class (FR-019).
- [ ] T081 [P] `DEVNOTES.md`: add a status-table entry for spec 025 (handles, parameter hook, vocabulary, registry, light order; DB-008 resolved; macOS validation date; Linux validation pending until T087's results are in). Link `DOMAIN_BOUNDARIES.md` and `specs/025-graphics-state-handles/`.
- [ ] T082 [P] `CLAUDE.md` — the maintainer explicitly authorized this edit (maintainer, 2026-10-26): Known gotchas item 24, Recent Changes, Active Technologies, Architecture and Testing only. **Do not touch the "Spec-kit agent rules" section.**
  - **Known gotchas**: add item 24 covering:
    - Graphics State stores `ShaderGroupRef`/`MapRef` handles and resolves them only through the resolving domain's inline `static_cast` accessors (`shaderInstanceOf`, `lightInstanceOf`, `photonMapOf`); State never includes `shader.h`, `photonMap.h`, `dsply.h` or `blobbyField.h`;
    - the slots are private; every bind goes through `bind*()`, which recomputes `usedParameters`; `RiWorldBegin` uses `setDefaultSurface()`, which leaves the Surface/Bxdf record alone (FR-024);
    - the used-parameter logic lives in Shading (`shaderParameters.cpp`) behind `shaderParameterHooks.h`, installed in `rendererShadingServicesInit()` and **never** in `rendererDeclarations.cpp` (ribVector would fail to link); with no hook, `usedParameters` is 0 and `findParameter` is `nullptr`;
    - `CRenderer::retrieveDisplayChannel` is an out-of-line forwarder to the State-owned `CChannelRegistry`; keep it out of line;
    - `CAttributes` copies and `restore()` now keep light order (it used to reverse on every push); the unchecked light cast (DB-013) lives in `lightInstanceOf()`;
    - a State-only test that binds doubles must run with no `RiBegin` session open.
  - **Recent Changes** and **Active Technologies**: add a 025 entry each (as 024 has).
  - **Architecture**: one short paragraph on Graphics State's handle model; **Testing**: mention `ctest --test-dir build -L state`.
- [ ] T083 [P] Grep `DEVNOTES_DETAILS/*.md` and `docs/` for the old slot names and the CRenderer channel statics (`CRenderer::declaredChannels`, `declareDisplayChannel`, public `attributes->surface`) and update any text that describes them; no user-facing site page changes (rendering does not change).
- [ ] T084 [RUN] No-JIT build (spec edge case "Build options"), in a scratch directory inside the worktree, deleted afterwards: `cpujob 025-nojit-cfg cmake -B build-025-nojit -S . -DOPENRENDER_ENABLE_JIT=OFF`, then `cpujob 025-nojit-build cmake --build build-025-nojit --config Release -j4`, then `cpujob 025-nojit-test ctest --test-dir build-025-nojit -L "architecture|state|shading_parity" -j4 --output-on-failure`. Record the result.
- [ ] T085 [RUN] SC-005: re-time `Visual_teapot-motion-raytrace`, `Csg_Seam_freeform_nurbs` and `Csg_Seam_spherecube_reyes_1600`, each 5 times alone through `cpujob` (quickstart §0.6). Each median must fall within the validation.md §0.6 min–max band. Record min/median/max.
- [ ] T086 [RUN] Final macOS pass, with results in validation.md §8:
  - CP-a, CP-b, CP-c; `ctest -L state`, `-L shading_parity`, `-L visual -j4`, `-L '^determinism$' -j2`;
  - libshader_shading imports still **7**;
  - quickstart §3 greps: `git diff dfffba0 -- src/ri/texture/texture3d.cpp src/display/dsply.h src/ri/geometry/blobbyField.h` empty, no `shader.h`/`photonMap.h`/`dsply.h`/`blobbyField.h` include in `src/ri/state`;
  - the architecture report: no `DB-008`, DB-021 still present, no `STALE`;
  - `ctest -N -L 'state|shading_parity|rib_parse'` and the run show **zero** "Not Run (Disabled)" tests attributable to this spec: every `DISABLED` marker added in Phase 2 has been removed by its phase. Confirm this from the actual output of the `ctest -L ...` run (its "Not Run (Disabled)" lines and summary), not from `ctest -N` alone.
    - **The one test disabled before this spec**: `ShadingParity_IlluminationHookAllFalse` (label `shading_parity`) is `DISABLED` on master until GitHub #71 (`fix/jit-illumination-hook-false-if`, in progress) lands.
    - If #71 is merged into master before Phase 8, run this task on the 025 tree **merged with master** (a scratch merge in a scratch worktree or build, never on the 025 branch), where that test is enabled, and require zero disabled.
    - Otherwise list `ShadingParity_IlluminationHookAllFalse` as the **single known exception**, tied to #71, and require that it is the only "Not Run (Disabled)" line.
  - tick the checkboxes in this file.
- [ ] T087 Linux validation package (quickstart §4). The agent has **no Linux access**: write it into validation.md §9 and hand it to the maintainer.
  - **Sync**: an rsync block from the worktree to the Linux checkout excluding `build*/` (quote hosts as `${H}:path`; memory `feedback_gcc13_werror_linux_validation.md`, `reference_zsh_host_colon_modifier.md`), plus a `sha256sum` manifest of every changed file at `specs/025-graphics-state-handles/tools/025-linux-manifest.sha256`.
  - **Maestri steps**: a fresh `build-025` (Release, GCC 13, `-Werror`) built with `nice -n 19 cmake --build build-025 -j4`; then the quickstart §2 commands.
  - **Watch for**: `-Wdeprecated-enum-enum-conversion` at the vocabulary conversion sites; `-Wnull-dereference`/`-Warray-bounds` around the pattern-table container; `ctest -L architecture` passing with the platform-tagged lines unchanged (SC-001 on Linux).
  - **Baseline**: the same commands on `dfffba0` at the same time, so the Linux pass list is compared with a Linux baseline, not with macOS's.
  - **FR-025 on Linux**: run the affected tests (`tools/affected-regex.txt`) and compare every `MaxBlockAvgDiff` with the Linux `dfffba0` run. Any difference: list the scene, the old and new diffs and the reason (light order), and **stop for the maintainer**. Never re-render and never touch a threshold.
  - **Determinism sweep**: also `ctest -L '^determinism-sweep$' -j2 --output-on-failure` on Linux, compared with the same sweep on the Linux `dfffba0` baseline.
  - **Imports**: the `nm -u` intersection for libshader_shading is unchanged (spec 024's Linux value).
  - When the maintainer reports the results, append them to validation.md §9 and finalize the DEVNOTES entry (T081). Linux green is required before the maintainer merges; this task does not depend on the merge.
- [ ] T088 **Final checkpoint (PAUSE 8)**: stage every changed file with `git add` (never commit), confirm `git status` shows no unstaged change and no build artefact, summarize against the success criteria below, and wait for review. Then run `/speckit-converge`. #31 is closed by the maintainer's `Closes #31` in the merge commit.

---

## Dependencies & Execution Order

### Phase dependencies

| Phase | Depends on | Notes |
|---|---|---|
| 1 Baseline | nothing | runs before any source edit |
| 2 Tests first | 1 | no `src/` edit; ends in the maintainer's test approval |
| 3 Parameter hook | 2 | must precede 4: while the logic is in `attributes.cpp`, dropping `shader.h` would add a non-allowlisted edge |
| 4 Handles, maps, vocabulary | 3 | T030 (allowlist deletion) first; the tree is uncompilable from T036 to T049 |
| 5 Record and table | 4 | the `bxdf` slot needs `ShaderGroupRef` |
| 6 Registry | 4 | independent of 5 in code, ordered after it by the plan; both edit `attributes`-adjacent files only through State |
| 7 Light order | 4 (the helper edits the handle-typed loops); ordered last of the code phases | the only output-changing phase |
| 8 Docs, Linux | 7 | |

### Within each phase

- Phase 2: T007, T008 → T009–T014, T016, T016a and T017 [P]; T015 → T016 and T016a; T018 after all of them; T019 → T020 → T021.
- Phase 3: T022 → T023 → T024 → T025 → T026 → T027 → T028 → T029 (T023 needs the hook header from T022).
- Phase 4: T030 → T031 → T032–T035; T036 → T037; T038; T039–T043 need T033/T034/T035/T036; T044 → T045; T046, T047, T048 [P] after T036; T049 after all of them; T050 → T051; T052, T053 → T054.
- Phase 5: T055 → T056 → T057 → T058; T059 → T060 → T061 (US3 and US4 both edit `attributes.h/.cpp`, so they are sequential); T062.
- Phase 6: T063 → T064 → T065 → T066 → T067 → T068 → T069 → T070.
- Phase 7: T071 → T072 → T073 → T074 → T075 → T076 → T077 → T078.
- Phase 8: T079–T083 [P]; T084, T085 any time after T078; T086 after T079–T085; T087 after T086; T088 last.

### User story map

| Story | Tasks |
|---|---|
| US1 (no cross-domain dependency) | T007, T008, T009, T013, T022, T024, T025, T028, T030, T031, T032, T033, T034, T035, T036, T037, T038, T042, T043, T044, T045, T046, T047, T049, T050, T051, T052, T053 |
| US2 (every scene renders as before) | T014, T015, T016, T016a, T020, T023, T026, T027, T039, T040, T041, T048, T057, T071, T072, T073, T074, T075, T076, T077 |
| US3 (Surface/Bxdf record) | T010, T017, T055, T056, T058 |
| US4 (pattern-node table) | T011, T059, T060, T061 |
| US5 (channel registry) | T012, T063, T064, T065, T066, T067, T068, T069 |
| Cross-cutting (no label) | T001, T002, T003, T004, T005, T006, T018, T019, T021, T029, T054, T062, T070, T078, T079, T080, T081, T082, T083, T084, T085, T086, T087, T088 |

### Requirement traceability

| Requirement | Tasks |
|---|---|
| FR-001, FR-004 | T009, T031, T036, T037 |
| FR-002 | T035, T042, T046 |
| FR-003 | T022, T025, T034, T043, T047 |
| FR-005, FR-006, FR-007 | T016, T016a, T020, T022–T027, T039, T056, T057 |
| FR-008, SC-007 | T013, T032, T045, T051 |
| FR-009, SC-003 | T038, T052 |
| FR-010, FR-011, FR-024 | T010, T055, T056 |
| FR-012 | T055, T058 |
| FR-013, FR-014, FR-015 | T011, T059–T061 |
| FR-016 to FR-018, SC-008 | T012, T063–T069 |
| FR-019, FR-020, SC-001, SC-002, SC-009 | T030, T053, T079, T080 |
| FR-021 (red first) | T007–T020 (incl. T016a), T071 |
| FR-022, SC-004 | T029, T054, T062, T070, T075, T086 |
| FR-023 | T017, T058 |
| FR-025, SC-010 | T014, T071–T078, T087 |
| SC-005 | T085 |
| SC-006 | T007–T009, T050 |

---

## Parallel opportunities

| Phase | Parallel tasks |
|---|---|
| 1 | T002 alongside T001 |
| 2 | T009–T014, T016, T016a, T017 (test files); T015 before T016 and T016a |
| 4 | T032–T035 (four separate new files or headers); T041, T042, T043 (different files); T046, T047, T048 |
| 8 | T079–T083 (documentation) |

## Parallel example: Phase 2 tests first

```text
T009 tests/unit/state/test_state_isolation.cpp
T010 tests/unit/state/test_state_last_bound.cpp
T011 tests/unit/state/test_state_pattern_table.cpp
T012 tests/unit/state/test_state_channel_registry.cpp
T013 tests/unit/state/test_state_vocabulary.cpp
T014 tests/unit/state/test_state_light_restore.cpp
T016 tests/shading_parity/test_shader_parameter_equivalence_direct.cpp
T016a tests/shading_parity/test_shader_parameter_bxdf_ignored.cpp
T017 tests/unit/rib_parse/test_ribout_shader_order.cpp
```

## Implementation strategy

**MVP is Phases 1–4.** After Phase 4, Graphics State includes no other domain's header (DB-008's 7 edges are gone), the
shader-parameter logic is in Shading, the vocabulary is State's, and every render is byte-identical. That is the
remediation this spec exists for, and it unblocks specs 026 and 034.

The rest is incremental:
- Phase 5 gives specs 034 and 035 the `bxdf` slot, the last-bound record and the pattern table.
- Phase 6 moves the channel registry (the registry part of DB-021; the entry stays Open for spec 037).
- Phase 7 fixes the light-order reversal, in isolation, so that its effect is measured on its own.
- Phase 8 updates the registry and docs, validates without the JIT and on Linux.

Each phase leaves the tree green (apart from the compile-broken span inside Phase 4), so any PAUSE is a safe stopping point.
