---

description: "Task list for spec 024: shading services routing"
---

# Tasks: Shading Services Routing

**Input**: design documents in `specs/024-shading-services-routing/`.

**Prerequisites**:
- plan.md
- spec.md
- research.md (R1–R12, all measurements done)
- data-model.md
- contracts/renderer-services.md
- contracts/illumination-hook.md
- contracts/jit-op-abi.md
- quickstart.md

**Tests**: required. Constitution III (TDD) is non-negotiable. In every phase the tests are written first
and shown failing (red) before the implementation tasks that make them pass. The only tests that pass
from the start are the control scenes (`Visual_photon-parity`, `Visual_solar0`): they render the
interpreter against its own reference, and they stay green throughout.

**Organization**: the phases follow plan.md's sequence. **Registration comes first**, because every
shader load resolves its global variables through the services (research R5). Every phase ends in
two pauses:
- a **TEST-APPROVAL** pause after its tests-first block;
- a **review PAUSE** at its end, where the results are reported, changes are staged, and the
  maintainer reviews and commits.

The agent never commits. It stages with `git add` only.

**Working directory**: `/Volumes/Projects/Development/CLI/openrender-worktrees/024-shading-services-routing`.
The macOS build tree is `build-024`. Paths below are repo-relative.

**The implementing agent runs builds and tests itself.** Steps marked **[RUN]** are builds or test
runs the implementer executes and records, under the machine limits: one build or test job at a time
across the machine, and no other build or test running when it starts. Every build is
`nice -n 19 cmake --build build-024 --config Release -j4`, and every ctest run is
`nice -n 19 ctest --test-dir build-024 … -j4`. Never use a higher `-j`, or `-j` with no number.
A [RUN] line below that shows a bare `cmake --build`, `cmake -B` or `ctest` means this niced `-j4` form.

**Red tests that don't compile yet**: a new test that needs an API that doesn't exist yet is added
with `EXCLUDE_FROM_ALL`, so the main build stays green.
- Its red state is shown two ways: `cmake --build build-024 --target <t>` fails to compile, and the
  ctest entry reports "Not Run".
- The implementation task that provides the API removes `EXCLUDE_FROM_ALL`.

**Allowlist rules (FR-015, FR-016)**:
- The only allowlist change is deleting the 10 DB-001 lines named in T023.
- **Adding a line is never allowed.** If the check reports a new `VIOLATION`, the design is wrong:
  stop and report it.
- Metric values only go down.

**No task depends on the merge**:
- Registry entries move to Resolved with "spec 024" and the date the task is done.
- Issue #30 is closed by the maintainer's `Closes #30` line in the merge commit.
- #24 (DB-001) stays open.
- No task posts issue comments.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency on unfinished tasks).
- **[Story]**: US1–US3 from spec.md.
- **[RUN]**: a build or test run done by the implementer, one job at a time.

---

## Phase 1: Setup — baseline record (before ANY source edit)

**Purpose**: put the measured baselines in one place, and capture the full-suite result that every
later phase is compared against.

- [X] T001 Create `specs/024-shading-services-routing/validation.md` with a "Baselines" section.
  Copy these from research.md, each with its source file in `tools/`:
  - **Base commit**: the worktree HEAD (`690dbfc`), and where `build-024` lives.
  - **R1**: the `.rslo` line `solar #!Label2`; the bitcode counts `op_solar_begin` 0/1 and
    `op_solar_end` 1/1; the `solar0` red diff of 201.62.
  - **R3, macOS**: 12 "dynamically looked up" symbols (`tools/libshader-dynlookup-before-macos.txt`).
  - **R3, Linux**: `nm -D --undefined-only` gives 469 lines; direction-correct bindings give 91 lines.
    Their strong-`U` intersection is the same 12 (`tools/024-libshader-*-before-linux.txt`).
  - **R4**:
    - the macOS batch table: 4/30 crashes at the default thread count, 0/30 and diff 0.00 at `-t:1`;
    - Linux at `-t:1` against the macOS reference: 30/30 runs with diff 2.91, 0 crashes;
    - D = 2.91, so the threshold is 6;
    - the empty-map margin is 255.00;
    - the red `slo` run segfaults on both platforms (`tools/024-photon-parity-linux-summary.txt`).
  - **Architecture**: 18 DB-001 allowlist lines; the Shading metric is `9 5`, and TOTAL `742 117`.
- [X] T002 [RUN] macOS full-suite baseline on the unchanged tree:
  `nice -n 19 cmake --build build-024 --config Release -j4`, then
  `nice -n 19 ctest --test-dir build-024 -j4 --output-on-failure`, plus
  `nice -n 19 ctest --test-dir build-024 -L architecture -j4`.
  - Record in `validation.md` the total, every failing test, and which of them are the known
    intermittent ones: `ImagerRegressionRender`, `Parity_transparency-matte-aov`,
    `Parity_depthdefault`, `ShadingParity_AmbientAccumulation`.
  - `architecture` must be green.
  - Record that the `determinism`-labelled tests pass (SC-006).

**Checkpoint (PAUSE 1)**: the baselines are recorded. Stage `validation.md`, summarize, and wait for
review.

---

## Phase 2: Foundational — services registered for the whole session (FR-005)

**Purpose**: research R5 and contracts/renderer-services.md §"Registration lifetime". This blocks
every story: routing a lookup before this lands would break Imager and default shaders, which load
before the first `beginFrame()`.

### Tests first

- [X] T003 Write a shared test double, `tests/shading_parity/recording_services.h`.
  - `class CRecordingServices final : public CRendererServices` takes the previously installed
    services in its constructor and forwards **every** method to them. This includes the current
    `hiderFlags()`, which T019 removes from both.
  - It counts calls to `getFilter`, `getStepFilter`, `getGlobalID` and `retrieveVariable` in
    public counters. T026 adds `getDSO`.
  - Also add `class CStubServices final : public CRendererServices`, which returns fixed sentinel
    values (the box filter, the box step filter, global ID 4242, a static `CVariable`, and later a
    static `CDSO`) and trivial defaults for everything else.
- [X] T004 Write `tests/shading_parity/test_services_routing.cpp`. The executable is
  `test_shading_services_routing` and the test `ShadingServices_Routing`.
  - Register it in `tests/shading_parity/CMakeLists.txt` with label `shading_parity`, linking
    `openrender_common_flags ribRender libshader_shading openrendercommon`, with the same
    `ENVIRONMENT` as `ShadingParity_AmbientAccumulation`.
  - **One `RiBegin`/`RiEnd` session in the whole process**, because several sessions in one
    process are a known flaky pattern. The order:
    1. **Case A**: right after `RiBegin(RI_NULL)`, before `RiWorldBegin`,
       `CShadingContext::defaultServices() != nullptr`.
    2. Install a `CRecordingServices` wrapping the current default with `setDefaultServices`.
       T024 inserts **Case D** here: a shader load before `RiWorldBegin`.
    3. **Case C**: `RiWorldBegin()`, then assert `defaultServices()` is still the wrapper.
       `beginFrame()` must not overwrite it.
    4. Restore the original default, then `RiWorldEnd()` and `RiEnd()`.
    5. **Case B**: after `RiEnd()`, `defaultServices() == nullptr`.
  - **Expected red today**: A fails, because services are null until `beginFrame`. B fails, because
    they stay registered. C fails, because `beginFrame` reinstalls the real implementation.
  - Later sessions in the same process (spec Edge Cases) are covered by design only: `RiBegin`
    registers and `RiEnd` deregisters, and Cases A and B show both edges. Record this acceptance
    in `validation.md`.

**Checkpoint (TEST-APPROVAL 2a)**: [RUN] build, then
`nice -n 19 ctest --test-dir build-024 -R ShadingServices_Routing --output-on-failure`. Show A, B
and C red, record them in `validation.md`, stage, and wait for approval of the test.

### Implementation

- [X] T005 In `src/ri/render/rendererShadingServices.cpp`, add `void rendererShadingServicesShutdown()`,
  which calls `CShadingContext::setDefaultServices(nullptr)`, next to `rendererShadingServicesInit()`.
- [X] T006 Change `src/ri/render/renderer.cpp`:
  - in `CRenderer::beginRenderer()`, call `rendererShadingServicesInit()` right after
    `initDeclarations()`;
  - remove the call and its comment from `CRenderer::beginFrame()`, and move the forward
    declarations to the top of the file;
  - call `rendererShadingServicesShutdown()` as the **last** statement of `CRenderer::endRenderer()`.
- [X] T007 [RUN] Build, then run `ShadingServices_Routing` (must be green),
  `-R 'Visual_killeroo|Imager'` (Imager shaders load before `WorldBegin`), and the full suite.
  The results must equal T002's. Record them in `validation.md`.

**Checkpoint (PAUSE 2)**: services exist for the whole session and nothing else changed. Stage,
summarize, and wait for review and commit.

---

## Phase 3: User Story 1 — JIT light shaders honour the illumination hook (Priority: P1) 🎯 MVP

**Goal**: under the photon hider, a JIT light shader hands each illuminate/solar construct to the
hider exactly as the interpreter does (contracts/illumination-hook.md). Today the photon pass crashes
in `jitIlluminateBegin` instead (research R4), and the JIT never handles `solar()` with no arguments
(R1).

**Independent test**: `Visual_photon-parity-slo`, `Visual_solar0-slo`,
`ShadingParity_IlluminationHook` and `LibShader_IlluminationOperands` all pass, and the control
scenes stay green.

### Tests first

- [X] T008 [P] [US1] Add the test shaders, which the shader build compiles automatically through its
  glob:
  - `shaders/photonparity_view.sl`: copy `specs/024-shading-services-routing/tools/photonparity_view.sl`
    verbatim;
  - `shaders/solar0_light.sl`: copy `tools/solar0_probe.sl`, rename the shader to `solar0_light`,
    and change the header comment to "spec 024 FR-019: a light whose only construct is solar() with
    no arguments".
- [X] T009 [P] [US1] Add the scenes, keeping each tool file's header comment, with "research
  candidate" reworded to "parity test scene":
  - `examples/rib/tests/photon-parity.rib`, from `tools/photon-parity-rslo.rib`, with the map and
    display renamed to `photon-parity.gpm` and `photon-parity.tif`;
  - `examples/rib/tests/photon-parity-slo.rib`, from `tools/photon-parity-slo.rib`, using
    `photon-parity-slo.gpm` and `photon-parity-slo.tif`;
  - `examples/rib/tests/solar0.rib` and `solar0-slo.rib`, from `tools/solar0-{rslo,slo}.rib`, with
    `LightSource "solar0_light"` and displays `solar0.tif` and `solar0-slo.tif`.
- [X] T010 [US1] [RUN] Render the two references with the unchanged interpreter path, using
  `VISUAL_ENV`'s variables (quickstart §0.3 shows the environment).
  - **Photon parity**: delete any `photon-parity.gpm`, then run
    `orender -t:1 examples/rib/tests/photon-parity.rib`. Copy the result to
    `examples/rib/tests/references/photon-parity.tif`.
  - **Check it against the measurement**: diff it against `/tmp/024-pp-t1/work/ref.tif`, the
    reference R4's threshold was measured against, using `test_visual_render … 255`. Expect 0.00.
    If it isn't, stop and report; the threshold would no longer be justified.
  - **If that file is gone**: regenerate it first with
    `specs/024-shading-services-routing/tools/measure-photon-parity.sh "$PWD/build-024" 1 /tmp/024-pp-t1 -t:1`.
    Note in `validation.md` that the reference was regenerated on the post-Phase-2 tree, so the
    0.00 check compares against that run, not R4's original one.
  - **solar0**: `orender examples/rib/tests/solar0.rib`, copied to
    `examples/rib/tests/references/solar0.tif`.
- [X] T011 [US1] Register the scenes in `tests/visual/CMakeLists.txt`, next to the other `-slo`
  scenes:
  - Add a comment citing research R4: D = 2.91 (Linux against macOS, 30/30 runs), macOS run-to-run
    0.00 at `-t:1`, threshold 6, empty-map margin 255.00, and 4/30 crashes at the default thread
    count, hence `-t:1`.
  - `add_visual_test(photon-parity examples/rib/tests/photon-parity.rib photon-parity.tif photon-parity.tif 6 "-t:1")`
  - `add_visual_test(photon-parity-slo examples/rib/tests/photon-parity-slo.rib photon-parity-slo.tif photon-parity.tif 6 "-t:1")`
  - `add_visual_test(solar0 examples/rib/tests/solar0.rib solar0.tif solar0.tif)`
  - `add_visual_test(solar0-slo examples/rib/tests/solar0-slo.rib solar0-slo.tif solar0.tif)`
  - **Map cleanup**: one setup test per photon scene that deletes stale maps (research R4).
    - `add_test(NAME VisualSetup_photon-parity-clean COMMAND ${CMAKE_COMMAND} -E rm -f photon-parity.gpm WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests/visual/photon-parity)`
      with `FIXTURES_SETUP photon_parity_clean`.
    - The same for `-slo`, deleting `photon-parity-slo.gpm`, with `FIXTURES_SETUP photon_parity_slo_clean`.
    - Set `FIXTURES_REQUIRED` on `Visual_photon-parity` and `Visual_photon-parity-slo` to match.
  - The `-slo` suffix already puts the JIT scenes on the JIT-only list.
- [X] T012 [P] [US1] Write `tests/shading_parity/test_illumination_hook.cpp`
  (`ShadingParity_IlluminationHook`), following research R2 and contracts/illumination-hook.md.
  - **Probe**: `class CHookProbeContext : public CShadingContext` overrides `renderingLoop()` with an
    empty body. It overrides `illuminateBegin`, `illuminateEnd`, `solarBegin` and `solarEnd` to log
    each call, capturing `P[0..2]`, `N[0..2]` and `theta[0]` by value, or null.
  - **Fixtures**: four light shaders, `illuminate(P)`, `illuminate(P, N, a)`, `solar(D, a)` and
    `solar()`. Each is written to a temp dir and compiled with `OSHADER_BIN`, once to `.rslo` and
    once with `--jit` to `.slo`.
  - **Harness**: `RiBegin(RI_NULL)` then `RiWorldBegin`. Construct the probe with thread 0. Then
    `updateState()`, and set `Ps`, `Ns`, `costheta`, the tags and the counts for 4 points.
    `costheta` must be set so that today's red is an assertion, not a crash. Execute through
    `probe->execute(...)`, and destroy the probe before `RiWorldEnd`.
  - **Hook on** (`CRenderer::hiderFlags |= HIDER_ILLUMINATIONHOOK`):
    - the call log for each fixture must equal the contract table, and be identical for `.rslo`
      and `.slo`;
    - the begin call must leave the tags, `numActive`/`numPassive` and `L` untouched;
    - with every tag non-zero (all points inactive), the hook is still called and the tags and
      counts are left as the interpreter leaves them (spec Edge Case "all vertices inactive").
  - **Hook off**: no hook calls. `L`, the tags and the counts after begin must be equal across the
    backends. For `solar()`, `L == Ps` for active points.
  - **Toggle**: on the same loaded `.slo` (no reload), run hook-on, then clear the flag and run
    again. The second run must match the hook-off results; this checks that compiled code does not
    cache the hook state (spec Edge Case "Hook toggled between frames").
  - Register it only `if(OPENRENDER_HAVE_LLVM)`, with label `shading_parity`, links as in T004, and
    the `OSHADER_BIN` environment.
  - **Expected red**: every JIT hook-on row (no calls are logged), and the `solar()` hook-off row
    (`L` is not set).
- [X] T013 [P] [US1] Write `src/libshader/tests/test_illumination_emitter.cpp`
  (`LibShader_IlluminationOperands`, labels `libshader;compiler;unit`), modelled on
  `test_gate_hardening.cpp`. It drives `emitLLVMBitcode()` with hand-built `IRModule`s.
  - **Positive control**: `illuminate P #!L1` … `endilluminate` with a resolvable `P` compiles and
    writes a `.slo`.
  - **Label-only `solar`**: `solar #!L1` … `endsolar` compiles, **and** its bitcode contains
    `op_solar0_begin`.
  - **Negative**: `illuminate __no_such_var__ #!L1` and `solar __no_such_var__ 0 #!L1` make
    `emitLLVMBitcode()` return false, write no `.slo`, and log a diagnostic naming the mnemonic.
  - Register it in `src/libshader/tests/CMakeLists.txt` like `LibShader_GateHardening`, only with
    the JIT.
  - **Expected red**: the label-only bitcode has no `op_solar0_begin`, and both negative cases
    succeed silently.

**Checkpoint (TEST-APPROVAL 3a)**: [RUN] build, then
`ctest -R 'Visual_photon-parity|Visual_solar0|ShadingParity_IlluminationHook|LibShader_IlluminationOperands' --output-on-failure`.
- Red: `Visual_photon-parity-slo` (orender exits nonzero, the crash), `Visual_solar0-slo` (diff
  about 201), the JIT rows of the hook test, and the emitter cases.
- Green: `Visual_photon-parity` and `Visual_solar0`.

Record this in `validation.md`, stage, and wait for approval of the tests.

### Implementation

- [ ] T014 [US1] Add `bool rendererIlluminationHook() const` to `CShadingContext` in
  `src/libshader/shading/shading.h` and `shading.cpp`. It returns
  `SVC && SVC->hasIlluminationHook()`. Delete `rendererHiderFlags()` from both files.
- [ ] T015 [US1] In `src/libshader/shading/shaderOpcodes.h`, the six macros (`ILLUMINATE1EXPR_PRE`,
  `ILLUMINATE3EXPR_PRE`, `ILLUMINATEEND_PRE`, `SOLAR1EXPR_PRE`, `SOLAR2EXPR_PRE`, `SOLAREND_PRE`)
  test `this->rendererIlluminationHook()` in place of
  `this->rendererHiderFlags() & HIDER_ILLUMINATIONHOOK`.
- [ ] T016 [US1] In `src/libshader/shading/shading.cpp`, the JIT begin and end methods check
  `rendererIlluminationHook()` first. When it is true, each calls the hook below and returns,
  without reading `Ns`/`costheta` and without touching the tags, the counts or `L`:

  | Method | Hook call |
  |---|---|
  | `jitIlluminateBegin` | `illuminateBegin(from, nullptr, nullptr)` |
  | `jitIlluminate3Begin` | `illuminateBegin(from, axis, angle)` |
  | `jitSolarBegin` | `solarBegin(Nf, thetaf)` |
  | `jitIlluminateEnd` | `illuminateEnd()` |
  | `jitSolarEnd` | `solarEnd()` |

  No null guard on `costheta` anywhere: research R4 rules it out as a new backend difference.
- [ ] T017 [US1] Add `void jitSolar0Begin(int *tags, int n, int *numActive, int *numPassive)` to
  `CShadingContext` (`shading.h`/`shading.cpp`).
  - With the hook active it calls `solarBegin(nullptr, nullptr)`.
  - Otherwise it does what `SOLAR1EXPR_PRE` does: increment the tag of each tagged point, and set
    `L = Ps` for each active one, with no culling and no change to the counts.
  - Add the C-linkage `op_solar0_begin` to `src/libshader/shading/rslOps.h`/`rslOps.cpp`, forwarding
    to `activeContext()->jitSolar0Begin` (contracts/jit-op-abi.md).
- [ ] T018 [US1] In `src/libshader/compiler/llvmEmitter.cpp`'s `illuminate`/`solar` emission:
  - **New form**: a `solar` whose only operand is its label emits
    `op_solar0_begin(tags, numVerts, numActivePtr, numPassivePtr)`.
  - **Hard error**: any `illuminate`/`solar` instruction whose operands don't resolve to one of the
    four forms fails the emit with a diagnostic naming the mnemonic, the same way the
    `isHandledOpcode()` gate fails. It is no longer skipped silently.
  - Keep `++conditionalDepth`/`--conditionalDepth` as they are.
  - The build compiles every shipped shader with `--jit`. If the new error fires on one of them in
    T020, stop and report it with the shader and the instruction; never loosen the check.
- [ ] T019 [US1] Remove the dead code and the unused service method:
  - delete the six uncalled wrappers from `src/libshader/shading/rslBuiltins.h` and
    `rslBuiltins.cpp`: `rsl_illuminate_begin_1`, `rsl_illuminate_begin_3`, `rsl_illuminate_end`,
    `rsl_solar_begin_1`, `rsl_solar_begin_2`, `rsl_solar_end`;
  - remove `hiderFlags()` from `src/libshader/shading/rendererServices.h`,
    `src/ri/render/rendererServicesImpl.h` and `tests/shading_parity/recording_services.h`
    (plan decision 5).
- [ ] T020 [US1] [RUN] Build, then run:
  - the T-3a command (all green);
  - `ctest -L "shading_parity|libshader"`;
  - `ctest -L visual`;
  - the full suite, whose results must equal T002's apart from the new tests;
  - `grep -rn 'rsl_illuminate\|rsl_solar' src` and `grep -rn 'HIDER_ILLUMINATIONHOOK\|hiderFlags()\|rendererHiderFlags' src/libshader src/ri/render/rendererServicesImpl.h`,
    which must both print nothing (SC-007).
  - **Record the `Visual_photon-parity-slo` MaxBlockAvgDiff** (JIT against the interpreter
    reference). It has never been measured, because before the fix that run crashed. If it is above
    6, stop and report to the maintainer. Never edit the threshold.
  - **Record the size of `build-024/tests/visual/photon-parity-slo/photon-parity-slo.gpm`**
    (SC-005). It must exist and be non-zero; the interpreter's map at `-t:1` is 2 009 546 bytes
    (research R4). An absent or empty map fails SC-005 even if the image diff passes.

  Record the results in `validation.md`.
- [ ] T021 [P] [US1] Add a "Backend parity (spec 024)" section to `tests/visual/parity-thresholds.md`:
  - `Visual_photon-parity` and `-slo`: threshold **6** at `-t:1`. D = 2.91 is the Linux-against-macOS
    diff (constant over 30/30 runs), and the macOS run-to-run diff at `-t:1` is 0.00. The empty-map
    margin is 255.00 (≥ 18). The scenes run single-threaded because the default thread count
    crashed 4/30 times (the `CPhotonHider` race in BUGS.md).
  - `Visual_solar0` and `-slo`: the default threshold of 20. The red diff before the fix was 201.62.
- [ ] T022 [P] [US1] In `DEVNOTES_DETAILS/BUGS.md`, add a data point to the Open "Intermittent
  SIGSEGV in `CPhotonHider` photon-map construction" entry:
  - spec 024's parity scene crashed 4/30 runs at the default thread count on macOS (exit code 5, no
    map written) and 0/30 at `-t:1`;
  - Linux `-t:1` crashed 0/30;
  - the spec 024 parity tests therefore render at `-t:1`;
  - the race remains open.

**Checkpoint (PAUSE 3)**: JIT and interpreter agree under the photon hider and for `solar()` with no
arguments. Stage, summarize with the before/after table in `validation.md`, and wait for review and
commit.

---

## Phase 4: User Story 2 — The shading library reaches the renderer only through its services (Priority: P1)

**Goal**: the five renderer-core symbols leave `libshader_shading`'s imports, and no Shading source
includes `renderer.h` directly (FR-001–FR-004, FR-015, FR-016).

**Independent test**:
- `ctest -L architecture` is green with DB-001 at 8 lines;
- the macOS import list has 7 entries;
- `ShadingServices_Routing` and `ShadingServices_Lookup` pass.

### Tests first

- [ ] T023 [US2] Edit `tests/architecture/allowlist.txt`.
  - Delete exactly these 10 DB-001 lines:
    - the `include` lines from `@build/src/libshader/shading/rslo_generated.cpp`,
      `src/libshader/shading/executeMisc.cpp`, `shader.cpp` and `shaderPl.cpp` to
      `src/ri/render/renderer.h`;
    - the `link` lines `rslo_generated.cpp` → `retrieveVariable` and `getDSO`, `shader.cpp` →
      `getGlobalID`, and `shaderPl.cpp` → `getStepFilter`, `retrieveVariable` and `getFilter`.
  - Change the DB-001 section comment from `(18)` to `(8)`.
  - In `tests/architecture/metrics-baseline.txt`, set Shading `9 5` → `0 0`, and set the TOTAL
    references to the value measured after this spec's edits (expected about 740), with the reason
    stated in the file: this spec removes 9 Shading references, net of the +7 that #51 (`ad0ee50`)
    added on master (Geometry 76 → 78, Hiders 581 → 586), which the committed baseline never
    recorded (`archcheck` measured 749 against 742 on this base).
    - Leave the TOTAL distinct-members value (117) unchanged. It counts each member once across
      all classes rather than summing the rows, and `retrieveVariable` is still used by Geometry
      and the RIB parser.
    - T032 sets it to whatever `archcheck` reports.
  - **Expected red**: `ctest -L architecture` reports 10 `VIOLATION` lines.
- [ ] T024 [US2] Add **Case D** to `tests/shading_parity/test_services_routing.cpp` (FR-013, FR-014).
  - It goes at step 2 of T004's single session: with the `CRecordingServices` installed, after
    `RiBegin` and **before** `RiWorldBegin`.
  - Compile a fixture light with a `string __category = "key,fill"` parameter, and load it with
    `CRenderer::context->getShader(name, SL_LIGHTSOURCE, 0, NULL, NULL)`.
  - Assert that the wrapper counted at least one `retrieveVariable` (from the `.rslo` load) and at
    least 2 `getGlobalID` calls (one per category).
  - **Expected red**: both counts are 0, because the call sites still go straight to `CRenderer::`.
- [ ] T025 [P] [US2] Write `tests/shading_parity/test_service_lookup.cpp` (`ShadingServices_Lookup`,
  label `shading_parity`). It is added with `EXCLUDE_FROM_ALL` because the header doesn't exist yet.
  - It links `openrender_common_flags ribRender libshader_shading openrendercommon`.
    `ribRender` is needed because `libshader_shading` still leaves 7 imports unresolved (on Linux
    the load would fail without it, as `LibShader_McompConsistency` #44 does).
  - Install a `CStubServices` with `setDefaultServices`.
  - Assert that `libshader::lookupDSO`, `lookupFilter`, `lookupStepFilter`, `lookupGlobalID` and
    `lookupVariable` (from `serviceLookup.h`) return the stub's sentinels.
  - Restore `nullptr` afterwards.

**Checkpoint (TEST-APPROVAL 4a)**: [RUN] build.
- `ctest -L architecture` shows the 10 `VIOLATION` lines.
- `ShadingServices_Routing` Case D is red.
- `cmake --build build-024 --target test_shading_service_lookup` fails to compile.

Record this, stage, and wait for approval.

### Implementation

- [ ] T026 [US2] Move the DSO handle type and add the service (research R7, contracts/renderer-services.md).
  - Create `src/libshader/shading/dsoHandle.h` with `class CDSO` moved **verbatim** from
    `src/ri/render/renderer.h`, including `shadeop.h` for the `dso*Function` typedefs.
  - In `renderer.h`, include `dsoHandle.h` and drop the class.
  - Add an explicit `#include "dsoHandle.h"` to `src/libshader/shading/execute.cpp`, because
    `scriptFunctions.h` dereferences `CDSO`.
  - Add `virtual CDSO *getDSO(const char *name, const char *prototype) = 0;` to
    `src/libshader/shading/rendererServices.h`, with a forward declaration of `CDSO`.
  - Implement it in `src/ri/render/rendererServicesImpl.h` as
    `return CRenderer::getDSO(name, prototype);`.
  - In `tests/shading_parity/recording_services.h`, the recording double forwards and counts
    `getDSO`, and the stub returns a static `CDSO` sentinel.
- [ ] T027 [US2] Create `src/libshader/shading/serviceLookup.h` and `serviceLookup.cpp` in namespace
  `libshader`, and add them to `src/libshader/shading/CMakeLists.txt`.
  - Five functions: `CDSO *lookupDSO(const char *, const char *)`,
    `RtFilterFunc lookupFilter(const char *)`, `RtStepFilterFunc lookupStepFilter(const char *)`,
    `int lookupGlobalID(const char *)` and `CVariable *lookupVariable(const char *)`.
  - Each forwards to `CShadingContext::defaultServices()` when it is set.
  - When it is not, each returns the not-found value: `nullptr`, `RiGaussianFilter`,
    `RiGaussianStepFilter`, `0` or `nullptr`. It reports **no diagnostic yet**; Phase 5 adds it,
    red first.
  - Remove `EXCLUDE_FROM_ALL` from `test_shading_service_lookup`.
- [ ] T028 [US2] In `src/libshader/shading/rslo.y`:
  - replace the two `CRenderer::retrieveVariable` calls with `libshader::lookupVariable`, and the
    two `CRenderer::getDSO` calls with `libshader::lookupDSO`;
  - include `serviceLookup.h` and `dsoHandle.h`;
  - remove `#include "renderer.h"`.
- [ ] T029 [P] [US2] In `src/libshader/shading/shader.cpp`, replace `CRenderer::getGlobalID` with
  `libshader::lookupGlobalID`. Keep today's storing behaviour; Phase 5 changes it. Remove
  `#include "renderer.h"`. Add only the specific Shading, Graphics State or infrastructure headers
  the file then needs, never `renderer.h` again.
- [ ] T030 [P] [US2] In `src/libshader/shading/shaderPl.cpp`, route the two `getFilter` calls, the
  `getStepFilter` call and the `retrieveVariable` call through the helpers. Remove
  `#include "renderer.h"`, with the same header rule as T029.
- [ ] T031 [P] [US2] In `src/libshader/shading/executeMisc.cpp`, remove `#include "renderer.h"`,
  which is unused. Same header rule as T029.
- [ ] T032 [US2] [RUN] Build, then run:
  - `ctest -L architecture`: green, with no `STALE` and no `VIOLATION`, and the metric reporting
    Shading `0 0`. If it reports `NOTE metric … < baseline` for TOTAL distinct, lower that value in
    `metrics-baseline.txt` to the reported one and re-run. It may only go down;
  - `ShadingServices_Routing` and `ShadingServices_Lookup`: green;
  - the full suite: equal to T020's results;
  - the macOS symbol check from quickstart §1.1, saved as
    `specs/024-shading-services-routing/tools/libshader-dynlookup-after-macos.txt`. It must have
    exactly 7 lines, the Hiders/Texturing set (SC-001);
  - `grep -rn 'include "renderer.h"' src/libshader`, which must print nothing (SC-002).

  Record the results in `validation.md`.

**Checkpoint (PAUSE 4)**: DB-001 is down to 8 edges and 7 imports. Stage, summarize, and wait for
review and commit.

---

## Phase 5: User Story 3 — Shaders load and bind identically, with or without a renderer (Priority: P2)

**Goal**: with no services installed, every routed lookup warns once per kind and returns "not
found", and a missing category is skipped (FR-006, FR-014, SC-009). Registration timing (FR-005,
FR-013) is already covered by Phases 2 and 4.

**Independent test**: `ShadingServices_NoServices` passes.

### Tests first

- [ ] T033 [US3] Write `tests/shading_parity/test_no_services.cpp`. The executable is
  `test_shading_no_services` and the test `ShadingServices_NoServices`, with label
  `shading_parity`. Add it with `EXCLUDE_FROM_ALL` until T034.
  - It runs in its own process: no `RiBegin`, and **no `CShadingContext` constructed** (#53).
  - It links `openrender_common_flags ribRender libshader_shading openrendercommon`. `ribRender`
    only resolves `libshader_shading`'s remaining imports; the test never calls into it.
  - Install a counting `CDiagnosticSink` with `installDiagnosticSink()`.
  - Call each of the five helpers **twice**. Assert the not-found result every time.
  - Assert exactly **5** warnings: one per kind, each `CODE_NOTSTARTED`, with severity warning.
  - Assert that `libshader::parseCategories("key,fill", out)` returns 0 entries, with
    `out[0] == 0`.
  - **Expected red**: it doesn't compile (`parseCategories` doesn't exist), and the warning count
    would be 0.

**Checkpoint (TEST-APPROVAL 5a)**: [RUN] `cmake --build build-024 --target test_shading_no_services`
fails to compile. Record this, stage, and wait for approval.

### Implementation

- [ ] T034 [US3] In `src/libshader/shading/serviceLookup.cpp` and `serviceLookup.h`:
  - Add one `std::atomic<bool>` per kind. The first lookup of a kind made with no services reports
    `warning(CODE_NOTSTARTED, "renderer services are not installed: %s lookup of \"%s\" returns not found\n", kind, name)`.
    Later lookups of that kind are silent.
  - Add `int parseCategories(const char *categoryString, int *out)`. It tokenizes on `",\t "` as
    `shader.cpp` does today, appends only non-zero IDs from `lookupGlobalID`, terminates the list
    with 0, and returns the count.
  - Use it in `src/libshader/shading/shader.cpp`'s category loop, so 0 is never stored before the
    terminator.
  - Remove `EXCLUDE_FROM_ALL` from `test_shading_no_services`.
- [ ] T035 [P] [US3] In `src/libshader/include/openrender/RSLShading.h` and `RSLShadingState.h`,
  replace the "falls back to CRenderer::* globals" wording for these lookups with the no-services
  behaviour from contracts/renderer-services.md §"With no services installed".
- [ ] T036 [US3] [RUN] Build, then run `ShadingServices_NoServices` (green), `-L shading_parity`
  and the full suite. Record the results in `validation.md`.

**Checkpoint (PAUSE 5)**: every story is complete. Stage, summarize, and wait for review and commit.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [ ] T037 [P] Update `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` (FR-017, research R12):
  - **DB-001** (stays Open):
    - Evidence: the 7 remaining symbols.
    - "Measured by the architecture check": 8 edges; the 4 `renderer.h` includes are gone.
    - History: "spec 024, <date>: the five renderer-core lookups go through `CRendererServices`;
      12 → 7 imports, 18 → 8 edges".
    - Drop the spec-024 items from Direction.
  - **DB-012**: move it under `## Resolved` as "Resolved by spec 024
    (`specs/024-shading-services-routing/`), <date>". Its tracking line names only #30. Summarize
    the fix: one hook query for both backends; the JIT begin/end functions hand off to the hider;
    `op_solar0_begin`; the wrappers deleted; the photon parity test. The 029 (#35) coupling is
    already tracked under DB-005, so it is dropped from DB-012 without loss; say so in the
    Resolved text.
  - **DB-018**: add to Direction the explicit ray type / hit mode in `ShaderGlobals` (spec 026,
    #32), formerly listed under DB-012.

  `<date>` is the date this task is done. Do not refer to the merge.
- [ ] T038 [P] Update `CLAUDE.md`:
  - shader counts: 104 → 106 `.rslo` (and `.slo` when the JIT is on), plus the object total quoted
    next to them;
  - a new Known-gotchas item covering:
    - services are registered at `RiBegin` and deregistered at `RiEnd`, never per frame;
    - the illumination hook is one query, `rendererIlluminationHook()`, for both backends, and the
      photon hider never sets `costheta`;
    - Shading reaches the renderer only through `serviceLookup`/`CRendererServices`;
  - the Testing section: mention `ctest -L shading_parity`.
- [ ] T039 [P] Update the documentation:
  - `DEVNOTES.md`: add a status-table entry for spec 024;
  - `DEVNOTES_DETAILS/OSHADER_UPDATES.md`: `op_solar0_begin`, the hard emitter error for unresolved
    illuminate/solar operands, and the removal of the `rsl_*` wrappers.
- [ ] T040 [RUN] Reduced-feature build in a scratch directory inside the worktree, deleted afterwards:
  - `nice -n 19 cmake -B build-024-nojit -S . -DOPENRENDER_ENABLE_JIT=OFF`;
  - `nice -n 19 cmake --build build-024-nojit --config Release -j4`;
  - `nice -n 19 ctest --test-dir build-024-nojit -L "architecture|shading_parity|visual" -j4 --output-on-failure`.

  The `-slo` scenes must be reported disabled, and the hook and emitter tests not registered.
  Record the results in `validation.md`.
- [ ] T041 Linux validation (research R3/R4; quickstart §0.4, §1.1). Prepare the package in
  `validation.md`; the coordinator **[RUN]**s it on the Linux host.
  - **Sync**: an rsync block from the worktree to the Linux checkout, excluding `build*/`, plus a
    `sha256sum` manifest of every changed file, at
    `specs/024-shading-services-routing/tools/024-linux-manifest.sha256`.
  - **Build**: a fresh `build-024` (Release, GCC 13, `-Werror`), built with
    `nice -n 19 cmake --build build-024 -j4`.
  - **Tests**: `ctest -L architecture`, `-L shading_parity`, `-L libshader`,
    `-R 'Visual_photon-parity|Visual_solar0'`, then the full suite.
    - The expected failures are only the known Linux ones from spec 023's T060.
    - Among them is `LibShader_McompConsistency` (#44), which still fails, now on
      `_ZTV19CTransmissionBundle`.
    - Record the Linux `Visual_photon-parity-slo` MaxBlockAvgDiff. The JIT-against-interpreter diff
      adds to the 2.91 cross-platform diff there, so if it is above 6, stop and report. Never edit
      the threshold.
    - Record the size of `build-024/tests/visual/photon-parity-slo/photon-parity-slo.gpm` (SC-005):
      it must exist and be non-zero (expect 2 009 546 bytes, as R4 measured on Linux).
  - **SC-001 on Linux**: the quickstart §0.4 commands. The `comm -12` intersection of strong-`U`
    symbols and direction-correct `libshader_shading.so.1 → libRIBRender.so.1` bindings must be
    exactly **7**, the Hiders/Texturing set. Save it as `tools/024-libshader-dynlookup-after-linux.txt`.

  Record the results. Linux green is required before the maintainer merges; this task itself
  doesn't depend on the merge.
- [ ] T042 [RUN] Final macOS pass: `ctest -L architecture` and the full suite.
  - `git diff df77ded -- tests/architecture/allowlist.txt` must show **only** the 10 deletions
    (plus the comment change).
  - `metrics-baseline.txt` may only show decreases.
  - The `determinism`-labelled tests pass (SC-006).
  - Tick the checkboxes in this file.

**Checkpoint (PAUSE 6, final)**: the feature is complete and Linux is confirmed. Stage, summarize,
and wait for review and commit; then run `/speckit-converge`. #30 is closed by the maintainer's
`Closes #30` in the merge commit.

---

## Dependencies & Execution Order

### Phase dependencies

| Phase | Depends on |
|---|---|
| 1 Setup | nothing; it runs before any source edit |
| 2 Foundational (registration) | 1; blocks 4 and 5 (routing needs services before the first frame) |
| 3 US1 hook parity | 2. It doesn't use the routed lookups, but T019 edits `recording_services.h`, which Phase 2 created (and `rendererServicesImpl.h`) |
| 4 US2 routing | 2; ordered after 3, because T026 edits the same interface files as T019 |
| 5 US3 no services | 4 (the helpers exist) |
| 6 Polish | 3, 4, 5 |

### Within each phase

- Tests first, then TEST-APPROVAL, then implementation, then the [RUN] verification and the review
  PAUSE.
- In Phase 3: T014 → T015 (macros use the new query); T016/T017 → T018 (the emitter needs the op);
  T019 after T016.
- In Phase 4: T026 → T027 → T028–T031.

### Parallel opportunities

| Phase | Parallel tasks |
|---|---|
| 3 | T008, T009, T012, T013 (tests); T021, T022 (docs, after T020) |
| 4 | T025 alongside T023/T024; T029, T030, T031 (disjoint files, after T027) |
| 5 | T035 alongside T034 |
| 6 | T037, T038, T039 |

## Parallel example: Phase 3 tests first

```text
T008 shaders/photonparity_view.sl, shaders/solar0_light.sl
T009 examples/rib/tests/photon-parity{,-slo}.rib, solar0{,-slo}.rib
T012 tests/shading_parity/test_illumination_hook.cpp
T013 src/libshader/tests/test_illumination_emitter.cpp
```

## Implementation Strategy

**MVP is Phases 1–3.** Services live for the whole session, and the user-visible defect is fixed:
JIT lights no longer crash the photon pass, and `solar()` with no arguments works. DB-012 is fully
addressed then; its registry move happens in T037.

The rest is incremental:
- Phase 4 removes the renderer-core coupling: DB-001 goes from 12 to 7 imports and from 18 to 8
  edges.
- Phase 5 makes standalone use safe.
- Phase 6 updates the registry and docs, and validates without the JIT and on Linux.

Each phase leaves the tree green, so any PAUSE is a safe stopping point.
