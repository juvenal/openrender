# Tasks: JIT/Interpreter Parity Follow-ups (post-011)

**Branch**: `012-jit-parity-followups` | **Date**: 2026-08-25 | **Input**: Design documents from `specs/012-jit-parity-followups/`

**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md), [data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md)

**Worktree**: `/Volumes/Projects/Development/CLI/openrender-worktrees/012-jit-parity-followups` —
run every command from here. Do **not** `cd` to the main checkout.

**Tests are REQUIRED**: Constitution Principle III (Test-Driven Development) is
NON-NEGOTIABLE and [plan.md](./plan.md) carries a per-story Red/Green mapping.
Every story's Red artifact is a task in this file and precedes its Green tasks.

---

## Format: `[ID] [P?] [Story?] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: `[US1]`, `[US2]`, `[US3]` — which user story the task serves
- Every task names an exact file path or an exact command target

## Standing rules (apply to every task below)

- **No automatic commits.** Commit only when the maintainer explicitly asks.
- **`.slo` staleness**: `stat` every `.slo` against both the `oshader` binary
  and the emitter/runtime sources before trusting any `-slo` result. A green
  `-slo` run after an emitter change proves nothing unless the bitcode
  postdates the edit and the rebuild.
- **No pre-existing reference image is regenerated** (SC-007, admits no
  exceptions). New coverage arrives only as new scenes with new references.
- **`ri` depends on `libshader_shading`, never the reverse.**

## Hard serialization points — never mark these `[P]`

Reproduced from [plan.md](./plan.md) § Execution Model so they cannot be lost
in task-level optimism:

1. **Phase 2 (Stream 0) in full**, before anything lands.
2. **Every `perf-manual` timing run** — exclusive, quiescent machine (T007, T008, T037).
3. **`.slo` regeneration after any emitter or shading-runtime change** (T033, T045),
   with a `stat` check before every `-slo` verification.
4. **The US1 STOP** (T015), before any `.rslo` interpreter *or* compiler source
   edit; and within US2, the §2.2 callee audit (T023–T024) precedes any collapse.
5. **The conditional US3 STOP** (T044), triggered only if the shared entry point
   cannot preserve both backends' observable behaviour.
6. **The FR-006 active/inactive discrimination check** (T035) — it gates T036 and
   may itself have to author new coverage. "No scene covers it" is not a
   passing outcome.

---

## Phase 1: Setup (shared infrastructure)

**Purpose**: Make the worktree buildable and its shader artifacts trustworthy.
The worktree has no `build/` directory yet — nothing can be compiled or measured
until this phase completes.

- [ ] T001 Configure and build the unchanged binary in the worktree: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release`, per `COMPILING.txt`
- [ ] T002 Run the `.slo` staleness audit from [quickstart.md](./quickstart.md) P1: `stat -f '%Sm %N' -t '%F %T' shaders/*.slo build/src/oshader/oshader src/libshader/compiler/llvmEmitter.cpp src/libshader/shading/rslOps.cpp | sort`, and record which `shaders/*.slo` predate `oshader` or the emitter/runtime sources
- [ ] T003 Regenerate every stale artifact found in T002 with `build/src/oshader/oshader --jit -o shaders/<name>.slo shaders/<name>.sl` (using `SHADERS_INCLUDE="$(pwd)/shaders/includes"` for shaders that `#include` `.slh` headers — **not** `-I`, which mis-parses with `-o`), then refresh the matching copies under `openrender/shaders/`

**Checkpoint**: `build/src/orender/orender` and `build/src/oshader/oshader` exist; no `.slo` predates its inputs.

---

## Phase 2: Foundational — Stream 0 baseline (BLOCKING)

**Purpose**: Capture every "before" number that SC-003, SC-004, SC-006, and
SC-007 are defined against. If any story's change lands first, its result
silently becomes the next story's "before" and those criteria can no longer be
evaluated.

**⚠️ CRITICAL**: No task in Phase 3, 4, or 5 may start until this phase completes.
**No task in this phase is `[P]`** — T007 and T008 additionally require an
exclusive, quiescent machine.

- [ ] T004 Create the shared recording file `specs/012-jit-parity-followups/measurements.md` (every later measurement task appends to it: T006, T007, T008, T012, T013, T034, T037, T044, T046, T053) with one section heading per phase, then capture the pristine compiler/unit baseline: `ctest --test-dir build -L libshader --output-on-failure 2>&1 | tee /tmp/base-libshader.txt`, and record the exact pass/fail set there (pre-existing failures belong in the baseline, not the blocker list)
- [ ] T005 Capture the pristine visual baseline: `ctest --test-dir build -L visual --output-on-failure 2>&1 | tee /tmp/base-visual.txt`, recording the exact pass/fail set
- [ ] T006 Record the same-configuration image noise floor into `specs/012-jit-parity-followups/measurements.md` — SC-007's "within noise" is undefined without it. Method, verbatim from `specs/011-jit-opcode-parity/tasks.md:222` (T048) and `lessons-learned.md:392-396`: render the *same unedited binary* twice for each stochastic raytrace scene, then compare the two runs with the project's 8×8 block-averaged diff metric (the same metric `tests/visual/CMakeLists.txt` uses); record per scene the **max block-avg** and **mean** of that same-binary pair. Spec 011's reference figure for the raytrace probe was max 39.375 / mean 0.0193 — expect the same order of magnitude, and treat any later before/after diff at or below the pair as noise
- [ ] T007 Establish the run-to-run **variance** baseline (does not exist today; spec 011 reported single-run ratios only — [research.md D7](./research.md)) on a quiescent machine: `for i in 1 2 3 4 5; do ctest --test-dir build -L perf-manual --output-on-failure 2>&1 | tee /tmp/perf-var-$i.txt; done`, then record per-scene min/max/spread of the JIT-to-interpreter ratio in `specs/012-jit-parity-followups/measurements.md`
- [ ] T008 Capture pre-change per-scene ratios in the **same quiescent session** as T007: `ctest --test-dir build -L perf-manual --output-on-failure 2>&1 | tee /tmp/perf-before.txt`, and fill the six-row density table from [quickstart.md](./quickstart.md) § 0.3 (`sphere-cfrom`/`show_st_hsv` = near-zero control, `sphere-ctransform`/`show_ctransform` = low, `sphere-matrixops`/`matrix_ops_probe` = high, `sphere-comparisonlogic`/`comparison_logic_probe` = high, `sphere-arrayops`/`array_ops_probe` = highest, `sphere-gather`/`gather_named_probe` = mixed) in `specs/012-jit-parity-followups/measurements.md`, recording each scene's uniform-computation density alongside its ratio so SC-004's classification is auditable rather than retrofitted

**Checkpoint**: Baselines exist for regression (T004, T005), image noise (T006), timing variance (T007), and timing level (T008). All three stories may now proceed in parallel.

---

## Phase 3: User Story 1 — Varying-index `uniform string` array read renders instead of crashing (Priority: P1) 🎯 MVP

**Goal**: A shader reading a `uniform string` array element at a varying,
in-range index renders to completion under the interpreter backend with the
correct per-point element selection, instead of terminating the whole render
(FR-001, FR-002, FR-003).

**Independent Test**: Render the new probe scene with the interpreter backend.
Before the fix it terminates abnormally; after the fix it completes, the
selected-element behaviour matches a hand-computed expectation and the
uniform-indexed equivalent, and the new scene passes as an executing regression
test while every pre-existing reference image stays untouched.

### Red — reproduction (FR-002); this IS the authorization for the fix

- [ ] T009 [P] [US1] Write the minimal probe shader `shaders/usfroma_probe.sl` performing a varying-index read of a fixed-length `uniform string` array consumed **inline** in an expression — the shape recorded at `specs/011-jit-opcode-parity/triage-results.md:85`, `if (usarr[findex] == "a")`. The read must be inline because `rsloStringSpecifier` (`src/libshader/compiler/rslo.y:342-347`) forces `SLC_UNIFORM` onto a bare `string`, so no RSL string *variable* can hold a varying value
- [ ] T010 [P] [US1] Author the scene pair `examples/rib/tests/sphere-usfroma-reyes.rib` (pins `Option "shaderformat" "rslo"`) and `examples/rib/tests/sphere-usfroma-reyes-slo.rib` (pins `"slo"`), modelled on the existing `sphere-arrayops-*` scenes
- [ ] T011 [US1] Compile the probe to bytecode: `build/src/oshader/oshader -o shaders/usfroma_probe.rslo shaders/usfroma_probe.sl`
- [ ] T012 [US1] Reproduce the crash 5×: `for i in 1 2 3 4 5; do <render command> examples/rib/tests/sphere-usfroma-reyes.rib; echo "run $i exit=$?"; done`, recording the exit status of each run in `specs/012-jit-parity-followups/measurements.md`
- [ ] T013 [US1] Record the observed reproduction rate against SC-001 in `specs/012-jit-parity-followups/measurements.md` — 5/5 abnormal termination is the expected result; if intermittent, record the rate (SC-001 admits any non-zero pre-fix failure rate paired with a 100% post-fix pass rate)

### Root cause and approval gate

- [ ] T014 [US1] Diagnose the root cause read-only across `src/libshader/shading/execute.cpp` (interpreter handling of the `usfroma` instruction) and `src/libshader/compiler/rslo.y` + `src/libshader/compiler/expression.cpp` (which instruction form the compiler chooses), and determine which side is at fault. **Change no source in this task**
- [ ] T015 [US1] **🛑 MANDATORY STOP — maintainer approval required.** Present: the T012/T013 reproduction evidence recorded in `specs/012-jit-parity-followups/measurements.md`, the confirmed root cause, which side it lies on (interpreter — `src/libshader/shading/execute.cpp` — or compiler — `src/libshader/compiler/rslo.y` / `expression.cpp`), and the narrowest proposed change naming the exact file and line range. If the fix is compiler-side, the presentation MUST additionally state that it alters the meaning of shader artifacts already compiled by the previous compiler, for **both** backends at once (FR-002, FR-011). Wait for explicit confirmation. **Not `[P]` under any circumstance**

### Green — fix and permanent coverage

- [ ] T016 [US1] Apply the approved narrowest change in the approved file (`src/libshader/shading/execute.cpp` if interpreter-side, `src/libshader/compiler/rslo.y` or `src/libshader/compiler/expression.cpp` if compiler-side). No incidental refactoring (FR-011)
- [ ] T017 [US1] Rebuild (`cmake --build build --config Release`) and regenerate **both** probe artifacts: `build/src/oshader/oshader -o shaders/usfroma_probe.rslo shaders/usfroma_probe.sl` and `build/src/oshader/oshader --jit -o shaders/usfroma_probe.slo shaders/usfroma_probe.sl`; if the fix was compiler-side, regenerate **every** `shaders/*.slo` and `shaders/*.rslo` plus the deploy-tree copies, because the compiler change altered their meaning
- [ ] T018 [US1] Re-run the reproduction 5× against `examples/rib/tests/sphere-usfroma-reyes.rib` and confirm 5/5 normal completion (SC-001), recording results in `specs/012-jit-parity-followups/measurements.md`
- [ ] T019 [US1] Verify the per-point element selection in `shaders/usfroma_probe.sl` matches a hand-computed expectation and the uniform-indexed equivalent, and that `examples/rib/tests/sphere-usfroma-reyes-slo.rib`'s output matches `examples/rib/tests/sphere-usfroma-reyes.rib`'s within the visual-regression tolerance (spec Acceptance Scenarios 1 and 2)
- [ ] T020 [US1] Generate **one new** reference image for the probe scene under `tests/visual/reference/` and register the scene pair via `add_visual_test` in `tests/visual/CMakeLists.txt`, following the existing `sphere-arrayops-*` entries. Modify no existing reference image (FR-003, SC-007)
- [ ] T021 [US1] Add the FR-003 deliberate-omission note to the spec 011 diagnostic shader that had the construct removed (`shaders/array_ops_probe.sl`), recording that the omission is intentional and that coverage now lives in `shaders/usfroma_probe.sl` / `examples/rib/tests/sphere-usfroma-reyes.rib`, so the removal is not later mistaken for an oversight
- [ ] T022 [US1] Run the full regression and diff against the Phase 2 baseline: `ctest --test-dir build -R usfroma --output-on-failure`, `ctest --test-dir build -L libshader --output-on-failure`, `ctest --test-dir build -L visual --output-on-failure`; confirm the new test passes (SC-002) and zero tests newly fail versus `/tmp/base-libshader.txt` and `/tmp/base-visual.txt` (SC-003), and confirm `git status` shows no modified file under `tests/visual/reference/` other than the one new image (SC-007)

**Checkpoint**: US1 is complete and independently deliverable — SC-001, SC-002, and SC-003's first before/after pair are satisfied. **This is the MVP.**

---

## Phase 4: User Story 2 — Uniform-dominated shaders are not slower under the JIT (Priority: P2)

**Goal**: For an instruction the compiler classified uniform, the JIT performs
the computation once rather than once per shading point, matching the
interpreter, at every family where the interpreter short-circuits — with zero
output change (FR-004 … FR-008).

**Independent Test**: Measure JIT and interpreter wall-clock per measurement
scene before and after, under identical settings, and compare ratios against the
T007 variance; independently confirm the rendered images did not move.

**Red**: already captured as T007 + T008 — the JIT-to-interpreter ratio
exceeding 1.0 on uniform-dense scenes *is* the failing evidence.

### Prerequisite — discharge the callee audit (contract §2.2, NOT yet discharged)

**⚠️ Not `[P]`, and blocks every collapse task below.** No instruction may be
collapsed at a family until its callee has been checked.

- [ ] T023 [US2] Scan `src/libshader/shading/rslOps.cpp` for every use of `n` outside a loop bound: `grep -nE "\bn\b" src/libshader/shading/rslOps.cpp | grep -vE "i < n|i<n|int n|\* n|n \*|n\)|numVerts"`, and resolve each candidate named in [contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §2.2 — `rslOps.cpp:803` (`*numPassive = n;`), `rslOps.cpp:1093`/`1102`/`1112` (`op_area`/`op_calculatenormal`/`op_depth`, derivative-dependent and likely **excluded**), `rslOps.cpp:836`/`842`/`888`/`894`/`903`/`914`/`920` (`DEFLIGHTFUNC`, already excluded by §4), `rslOps.cpp:555`/`584`/`1094`/`1103` (`if (n > 0 && ACTIVE(tags, 0))`)
- [ ] T024 [US2] Record the discharged audit in [contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §2.2 — each candidate either **cleared** (honours the `n == 1, tags == nullptr` guarantee) or **excluded with its reason** — and flip the section's "Audit status: NOT YET DISCHARGED" line. An op that cannot honour the guarantee is not a blocker; it is excluded and recorded, the same way `DEFLIGHTFUNC` is (FR-004's explicit-exclusion requirement)

### Implementation

- [ ] T025 [US2] Add the uniform-classification predicate to `src/libshader/compiler/llvmEmitter.cpp` — an instruction is uniform iff `dstDesc.stride == 0` (line 598) **and** every operand stride returned by `getVar()` (line 442) is `0`, the equivalence to the interpreter's `code->uniform` established in [research.md D1](./research.md)
- [ ] T026 [US2] Add the collapsed-call helper in `src/libshader/compiler/llvmEmitter.cpp` that, when the predicate holds, emits `n = 1` **and `tags = ptr null`** — never `n = 1` with a live tag pointer, which is the forbidden combination of [contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §2.3 (it renders correctly wherever vertex 0 is active and diverges only inside conditionals)
- [ ] T027 [P] [US2] Apply the collapse in the `DEFOPCODE` arithmetic dispatch — `emitBin`/`emitUn`/`emitTern` in `src/libshader/compiler/llvmEmitter.cpp:459-509` — skipping any callee excluded by T024
- [ ] T028 [P] [US2] Apply the collapse at the `DEFFUNC` builtin call sites in `src/libshader/compiler/llvmEmitter.cpp`, skipping any callee excluded by T024
- [ ] T029 [P] [US2] Apply the collapse at the `DEFSHORTFUNC` call sites in `src/libshader/compiler/llvmEmitter.cpp` (`environment` ×2, `shadow` ×2, `bake3d` — [contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §4), emitting the collapse **only** at sites whose callee T024 cleared. On a collapsed call the count is `1`, so there is no per-site count to derive; FR-007's bite here is "do not collapse where the grid width is semantically required", which is exactly T023/T024's disposition. Leave the *varying* path's existing count untouched — the pre-existing `numVerts`-vs-`numRealVertices` asymmetry is explicitly out of scope ([contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §3, [research.md D4](./research.md)) and must not be "fixed" under cover of this task
- [ ] T030 [P] [US2] Apply the collapse at the `DEFSHORTOPCODE` sites in `src/libshader/compiler/llvmEmitter.cpp`, or record that the family has zero real uses in the tree ([contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §4)
- [ ] T031 [US2] Confirm `DEFLIGHTFUNC` is left uncollapsed in `src/libshader/compiler/llvmEmitter.cpp` — the interpreter treats a uniform classification there as an error (`scripterror("Invalid uniform lighting call")`), so there is no run-once semantics to mirror
- [ ] T032 [US2] Write the complete FR-004 exclusion list — every dispatch site not converted, each with its reason — into `specs/012-jit-parity-followups/measurements.md`. Silent omission is explicitly not an acceptable outcome

### Green — output must not move

- [ ] T033 [US2] **Rebuild and regenerate — not `[P]`.** `cmake --build build --target oshader && cmake --build build --config Release`, then regenerate **every** `shaders/*.slo` and its deploy-tree copy, then re-run the T002 `stat` audit to confirm all bitcode postdates both the emitter edit and the `oshader` rebuild
- [ ] T034 [US2] Inspect the emitted IR for a uniform-dense shader — `build/src/oshader/oshader --jit -o /tmp/probe.slo shaders/array_ops_probe.sl`, then dump the module's `op_*` call sites — and confirm uniform-classified instructions call with `i32 1` **and** `ptr null` ([quickstart.md](./quickstart.md) § 2.3)
- [ ] T035 [US2] Verify per-point active/inactive semantics (FR-006) on a scene exercising a uniform instruction inside a conditional where **early points are inactive** — the single case that discriminates `tags = nullptr` from a live tag pointer, and the failure mode [contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §2.3 flags as "most likely to survive casual testing". First identify by name a visual-suite scene that already covers it. **"No existing scene covers it" is not an acceptable outcome** — if none does, author the coverage the same way US1 does: a new `shaders/uniform_in_conditional_probe.sl` (uniform-classified instruction inside an `if` whose predicate is false for the leading shading points), a new `examples/rib/tests/sphere-uniform-conditional-reyes.rib` / `-reyes-slo.rib` pair, ONE new reference image, and an `add_visual_test` entry in `tests/visual/CMakeLists.txt` — under the same SC-007 discipline (no existing reference image modified). Record the scene chosen or authored in `specs/012-jit-parity-followups/measurements.md`. **Not `[P]`** — it gates T036
- [ ] T036 [US2] Run `ctest --test-dir build -L visual --output-on-failure` and confirm zero differences above the T006 noise floor versus `/tmp/base-visual.txt` (FR-005, SC-007). **If a difference appears** it is most likely the known corner — for a uniform instruction inside an all-inactive block, today's JIT writes nothing while the interpreter writes once, so the change moves the JIT *onto* the reference. **Stop and present it for disposition**; never regenerate a reference image unilaterally. Also run `ctest --test-dir build -L libshader --output-on-failure` against `/tmp/base-libshader.txt` (SC-003's second before/after pair)

### Green — performance (exclusive machine)

- [ ] T037 [US2] **Exclusive, quiescent machine — not `[P]`; nothing else compiles or renders.** Run `ctest --test-dir build -L perf-manual --output-on-failure 2>&1 | tee /tmp/perf-after.txt`, then record in `specs/012-jit-parity-followups/measurements.md`: **SC-004** — ratio improves by more than that scene's T007 variance on 100% of scenes with meaningful uniform computation and zero scenes regress (`sphere-cfrom` showing no measurable change is a **conforming** outcome, not a failure); **SC-006** — the `sphere-arrayops` vs `sphere-cfrom` gap at identical scale narrows by more than the variance of that comparison (pass/fail, no magnitude floor; report the magnitude either way); **SC-005** — per scene, whether the JIT reached ≤90% of the interpreter, and where unmet, the residual dominant cost

**Checkpoint**: US2 is complete and independently deliverable — FR-004 … FR-008, SC-004, SC-005, SC-006, and SC-003's second before/after pair are satisfied.

---

## Phase 5: User Story 3 — Light iteration has exactly one implementation (Priority: P3)

**Goal**: The interpreter's `runLightsTemplate` macro and the JIT's
`CShadingContext::runLights`/`runCategoryLights` methods — hand-synced copies
that have already drifted in two places ([research.md D6](./research.md)) —
converge onto one implementation reproducing the **macro's** semantics exactly,
so the interpreter stays bit-unchanged (FR-009, FR-010, SC-008).

**Independent Test**: Render `illuminance`-using shaders under both backends
before and after and confirm output does not move, while confirming by
inspection that only one implementation remains.

**Red**: the before/after render set — T004 and T005 already captured it against
the unchanged binary; reuse those files.

**Process note**: US3 proceeds under FR-009's refactor exemption — **no STOP**,
because both backends discard the category on every form the JIT actually lowers
([contracts/light-iteration.md](./contracts/light-iteration.md) §3), so
convergence changes no observable behaviour. Full before/after verification is
still required.

- [ ] T038 [US3] Design the converged entry point in `src/libshader/shading/shading.cpp`, retaining a **category parameter** — the interpreter's 4-operand `illuminance` path (`IlluminationCat1`, `src/libshader/shading/shaderOpcodes.h:92`) genuinely uses it via `ILLUMINATION_RUNCATLIGHTS`; the no-category call passes the no-category value `0`, exactly as `NORMALLIGHT_PRE` already does ([contracts/light-iteration.md](./contracts/light-iteration.md) §2.3)
- [ ] T039 [US3] Implement the single light-iteration function in `src/libshader/shading/shading.cpp` reproducing the **macro form's** semantics: the macro's cache-validity predicate `!*aTag & !*lTag` (not the method's `tags[i] != 0 || lightingTags[i] == 0`) and the macro's category rule that a light with `categories == NULL` **is** included under `invertCatMatch`. The method's better cache predicate is deliberately **not** adopted here — it may be proposed afterwards as an interpreter change with its own STOP, once a single implementation exists to change ([contracts/light-iteration.md](./contracts/light-iteration.md) §2.2)
- [ ] T040 [US3] Route the interpreter's opcode bodies through the converged function by rewriting the `runLights` / `runCategoryLights` macro wrappers in `src/libshader/shading/execute.cpp:422-517` to call it, leaving `enterLight`/`exitLight` sequencing, the `currentLight` walk, `numActive`/`numPassive` bookkeeping, and `SHADERFLAGS_NONAMBIENT` handling unchanged ([contracts/light-iteration.md](./contracts/light-iteration.md) §2.4)
- [ ] T041 [US3] Route the JIT through the same function by rewriting `jitIlluminanceBegin` (`src/libshader/shading/shading.cpp:2059-2122`) to call it, keeping its `costheta` construction and uniform-P/N stride-3 broadcast where they are — those are JIT-side argument preparation, not light iteration
- [ ] T042 [US3] Delete the retired duplicate implementation from `src/libshader/shading/shading.cpp` (the `runLights`/`runCategoryLights` method pair at 1502-1558), leaving no copy, no re-deriving wrapper, and no "kept in sync" comment (SC-008)
- [ ] T043 [US3] Verify no shading math was re-derived (FR-010) by diffing the converged entry point in `src/libshader/shading/shading.cpp` against the retired macro body in `src/libshader/shading/execute.cpp` (git history for lines 422-517): the converged function must be the same computation both backends already performed, reached from one place
- [ ] T044 [US3] **🛑 CONDITIONAL STOP — only if triggered.** If T039–T042 show the single entry point cannot preserve both backends' observable behaviour simultaneously, stop: the spec's edge case routes US3 out of the refactor exemption and into the FR-011 process (empirical reproduction, maintainer approval, narrowest change). The affected sources are `src/libshader/shading/shading.cpp` and `src/libshader/shading/execute.cpp`. Absent the trigger, this task is a no-op — record that it did not fire in `specs/012-jit-parity-followups/measurements.md`. **Not `[P]`**
- [ ] T045 [US3] **Rebuild and regenerate — not `[P]`, and shared with T033.** `cmake --build build --config Release`, then regenerate every `shaders/*.slo` and the deploy-tree copies (this change touches the shading runtime), then re-run the T002 `stat` audit. If US2's T033 has already landed, whichever of the two lands second regenerates once for both
- [ ] T046 [US3] Verify the interpreter is **bit-unchanged** — zero differences, not "within noise" — across the `-rslo` visual scenes versus `/tmp/base-visual.txt`, and the JIT within the T006 noise floor across the `-slo` scenes: `ctest --test-dir build -L visual --output-on-failure` plus `ctest --test-dir build -L libshader --output-on-failure` against `/tmp/base-libshader.txt` (SC-003's third before/after pair, FR-009, FR-011)
- [ ] T047 [US3] Produce the SC-008 evidence: grep for the retired form's name across `src/` and confirm it has no remaining definition and no remaining call site, and confirm an `illuminance`-using shader renders correctly under both `shaderformat` settings. Record the grep output in `specs/012-jit-parity-followups/measurements.md`

**Checkpoint**: US3 is complete and independently deliverable — FR-009, FR-010, SC-008, and SC-003's third before/after pair are satisfied.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: FR-012 documentation reconciliation and feature-level acceptance.
T048–T051 are `[P]` against each other (distinct files) and may be drafted
concurrently with Phases 3–5, but are finalized only once their stories land.

- [ ] T048 [P] Update `DEVNOTES_DETAILS/BUGS.md` — move the `usfroma` interpreter-crash entry to Resolved with its root cause, the side it landed on, and the new probe scene that now covers it (FR-012)
- [ ] T049 [P] Update `DEVNOTES_DETAILS/OSHADER_UPDATES.md` — record the uniform-dispatch collapse (predicate, the `n=1`/`tags=nullptr` convention, the exclusion list from T032) and the light-iteration convergence (FR-012)
- [ ] T050 [P] Update `specs/011-jit-opcode-parity/lessons-learned.md` and its records to reflect SC-006's outcome under this feature — spec 011's unmet performance criterion and what this feature measured against it (FR-012)
- [ ] T051 [P] Update `DEVNOTES.md` — mark the three follow-up items resolved in `## Todos`, and reconcile the "Review in next steps — shading interpreter and LLVM JIT" subsection under `## Open Issues`, striking any item this feature closed and leaving the spec-013 candidates ([contracts/light-iteration.md](./contracts/light-iteration.md) §4) in place
- [ ] T052 Confirm the Hugo `site/` documentation tree is **not** updated — verify with `git status --short site/` returning no entries. Constitution Principle VII is exempted for this feature by the spec's Assumptions (internal engine defect-fix and performance work, not user-facing functionality the site's content model tracks)
- [ ] T053 Fill the Stage 4 feature-level acceptance table in [quickstart.md](./quickstart.md) with the evidence produced by Phases 2–5, mapping SC-001 … SC-008 and FR-012 to their recorded artifacts
- [ ] T054 Final verification sweep: `ctest --test-dir build -L libshader --output-on-failure` and `ctest --test-dir build -L visual --output-on-failure` clean versus the Phase 2 baseline, and `git status` shows exactly one added file under `tests/visual/reference/` and **zero modified** files there (SC-007, which admits no exceptions)

---

## Dependencies & Execution Order

### Phase dependencies

- **Phase 1 (Setup)** → no dependencies; the worktree has no `build/` yet, so nothing precedes it
- **Phase 2 (Foundational)** → depends on Phase 1. **BLOCKS Phases 3, 4, 5**
- **Phase 3 (US1, P1)** → depends on Phase 2; independent of Phases 4 and 5
- **Phase 4 (US2, P2)** → depends on Phase 2; independent of Phases 3 and 5
- **Phase 5 (US3, P3)** → depends on Phase 2; independent of Phases 3 and 4
- **Phase 6 (Polish)** → each task depends on its story landing; T053/T054 depend on all three

### User story dependencies

None. The spec's Assumptions state it outright: *"none blocks another, and each
is separately deliverable and separately verifiable."* The couplings that exist
are environmental, not logical:

- **T033 ↔ T045**: US2 and US3 both invalidate every `.slo`. Whichever lands
  second regenerates once for both and re-verifies — they must not regenerate
  concurrently.
- **T007/T008/T037**: every `perf-manual` run needs the machine idle, so no
  other story may build or render during them.
- **US1 landing before US2/US3 finish**: each stream verifies against its own
  captured "before" (`/tmp/base-*.txt` from Phase 2), so a landed US1 fix does
  not invalidate them — but a **compiler-side** US1 fix (T016) forces the
  full-tree artifact regeneration in T017, which every later `-slo` check must
  postdate.

### Within each user story

- **US1**: T009/T010 in parallel → T011 → T012 → T013 → T014 → **T015 STOP** → T016 → T017 → T018 → T019 → T020 → T021 → T022
- **US2**: T023 → T024 → T025 → T026 → T027/T028/T029/T030 in parallel → T031 → T032 → T033 → T034 → T035 → T036 → T037
- **US3**: T038 → T039 → T040 → T041 → T042 → T043 → T044 → T045 → T046 → T047

### Parallel opportunities

- **Across stories, after Phase 2**: all of Phase 3, Phase 4's T023–T032, and
  Phase 5's T038–T043 may proceed concurrently — authoring and unit verification
  only. Their rebuild/regenerate/measure tails (T033, T037, T045) serialize.
- **Within US1**: T009 (probe shader) and T010 (scene pair) touch different files.
- **Within US2**: T027, T028, T029, T030 are one independent edit per instruction
  family against the same predicate.
- **Within Polish**: T048, T049, T050, T051 touch four different documentation
  files and may be drafted concurrently, alongside any phase.

---

## Parallel Example: User Story 2

```text
# After T023–T026 (audit discharged, predicate and helper in place),
# launch the four family edits together — same file region, distinct
# dispatch sites, one shared predicate:
T027 [P] [US2] Collapse DEFOPCODE arithmetic — emitBin/emitUn/emitTern, llvmEmitter.cpp:459-509
T028 [P] [US2] Collapse DEFFUNC builtin call sites in llvmEmitter.cpp
T029 [P] [US2] Collapse DEFSHORTFUNC sites (environment ×2, shadow ×2, bake3d)
T030 [P] [US2] Collapse DEFSHORTOPCODE sites, or record zero real uses

# Then serialize: T031 (confirm DEFLIGHTFUNC excluded) → T032 (write exclusion
# list) → T033 (rebuild + regenerate ALL .slo) → T034 → T035 → T036 → T037.
```

---

## Implementation Strategy

### MVP first (User Story 1 only)

1. Phase 1 (T001–T003) — make the worktree buildable and its artifacts trustworthy
2. Phase 2 (T004–T008) — capture every "before"
3. Phase 3 (T009–T022) — reproduce, STOP, fix, cover
4. **STOP and validate**: the crash is gone in 5/5 runs, a new executing test covers it, no existing reference image moved

US1 alone is a shippable increment: it is the only one of the three that is an
outright failure a shader author can hit today with no workaround.

### Incremental delivery

1. Phases 1–2 → shared verification foundation
2. Phase 3 → US1 → validate → deliverable (SC-001, SC-002)
3. Phase 4 → US2 → validate → deliverable (SC-004, SC-005, SC-006)
4. Phase 5 → US3 → validate → deliverable (SC-008)
5. Phase 6 → documentation reconciliation and feature-level acceptance

Each story adds value without breaking the previous one, and each carries its
own before/after regression pair per SC-003.

### Parallel team strategy

With Phase 2 complete, three developers can work simultaneously:

- **Developer A** → US1 (`execute.cpp` or `rslo.y`, plus new shader/scene/reference)
- **Developer B** → US2 (`llvmEmitter.cpp`, plus the `rslOps.cpp` audit)
- **Developer C** → US3 (`shading.cpp`, `execute.cpp` light-iteration macros)

They coordinate at exactly three points: the shared `.slo` regeneration (T033/T045),
the exclusive-machine timing run (T037), and the US1 STOP (T015), which gates
only Developer A. Note that A and C both touch `src/libshader/shading/execute.cpp`
— A at the array-access opcode, C at the light-iteration macros — so their edits
must be coordinated even though they are logically independent.

---

## Notes

- `[P]` marks tasks touching different files with no dependency on an incomplete task
- `[Story]` labels map every implementation task to its user story for traceability
- Each user story is independently completable and testable
- Verify a story's independent test criteria before moving to the next
- **Never commit automatically** — commit only when the maintainer explicitly asks
- Stop at any checkpoint to validate before proceeding
