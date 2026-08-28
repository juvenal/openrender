# Tasks: JIT/Interpreter Shading Parity Fixes

**Input**: Design documents from `/specs/014-jit-shading-parity/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Constitution Principle III (NON-NEGOTIABLE, TDD) and the Constitution
Check's explicit TDD sequencing note require tests before the corresponding
fix lands. All three regression-guard tiers (table-parity, gating-condition,
differential-oracle — research.md D6) are included below, written before
their corresponding implementation task.

**Organization**: Tasks are grouped by user story. US1 and US2 are both
Priority P1 and share a single fix mechanism (`computeUsedParameters`) per
research.md D2 — see the note under Dependencies before assuming either
phase is independently implementable.

## Format: `[ID] [P?] [Story] Description`
- **[P]**: Can run in parallel (different files, no dependency on an
  incomplete task)
- **[Story]**: Which user story this task belongs to (US1, US2, US3, US4).
  Omitted for Setup/Foundational/Polish tasks.

---

## Phase 1: Setup

- [X] T001 Create `tests/shading_parity/` directory with a `CMakeLists.txt`
  mirroring `tests/imager/CMakeLists.txt`'s pattern exactly: executables
  link `openrender_common_flags ri libshader_shading openrendercommon`,
  include dirs `${CMAKE_SOURCE_DIR}/src` + `${CMAKE_BINARY_DIR}`,
  `set_tests_properties(... ENVIRONMENT "SHADERS=${CMAKE_SOURCE_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")`,
  and register `add_test` entries under a new `shading_parity` ctest label
  (contracts/differential-oracle-contract.md). Leave it with no
  `add_executable` yet — those are added per-story below.
- [X] T002 Add `add_subdirectory(shading_parity)` to `tests/CMakeLists.txt`
  (alongside the existing `framebuffer`/`imager`/`preview`/`visual`/`shaders`
  entries at lines 50-54).
- [X] T003 [P] Add a new executable + `add_test` block to the existing
  `src/libshader/tests/CMakeLists.txt` for
  `test_libshader_used_parameters_table` (tier 1), linking
  `libshader_compiler_obj`/`libshader_compiler` + `openrendercommon` only
  (no `ri`, matching `test_libshader_compiler`'s existing linkage pattern),
  registered as ctest `LibShader_UsedParametersTable` with
  `LABELS "libshader;compiler;unit"`.
- [X] T004 [P] Add a second executable + `add_test` block to
  `src/libshader/tests/CMakeLists.txt` for
  `test_libshader_used_parameters_gating` (tier 2), same linkage as T003,
  registered as ctest `LibShader_UsedParametersGating` with
  `LABELS "libshader;compiler;unit"`.

**Checkpoint**: build graph has empty slots for all three test tiers; no
test source files exist yet.

---

## Phase 2: Foundational (blocking prerequisites)

**Purpose**: Infrastructure every user story's fix depends on. No user story
work can start until this phase is complete.

- [X] T005 In `src/libshader/compiler/CMakeLists.txt`, add
  `target_include_directories(libshader_compiler_obj PRIVATE ${CMAKE_SOURCE_DIR}/src/libshader/shading)`
  (header-only; the two libraries have zero link dependency in either
  direction today — confirm this stays true after the addition).
- [X] T006 In `src/libshader/compiler/llvmEmitter.cpp`, add a local X-macro
  re-expansion (mirroring the precedent at
  `src/libshader/shading/rslo_code.h:31-49`) that `#include`s
  `shaderFunctions.h`/`shaderOpcodes.h`/`giFunctions.h`/`giOpcodes.h` with
  locally redefined `DEFOPCODE`/`DEFFUNC`-family macros, capturing
  `(text, params)` pairs into a new static table (data-model.md "Opcode/
  Function Bit Table"). Do not wire it into `usedParameters` computation
  yet — this task only builds and exposes the table.
- [X] T007 [P] Write `src/libshader/tests/test_used_parameters_table.cpp`
  (tier 1, contracts/table-parity-contract.md): assert every `(text, params)`
  pair in T006's new table matches the corresponding interpreter header
  entry exactly, re-derived at test-run time. Confirm it builds against the
  T003 CMake slot and passes immediately (T006's table has no drift by
  construction — this test is not expected to start Red, per
  table-parity-contract.md's Non-goals: it cannot see the gating-condition
  bug this feature fixes).

**Checkpoint**: shared X-macro table exists, exported, and guarded by a
passing tier-1 test. `usedParameters` computation itself is still
completely unfixed — every user story phase below depends on this.

---

## Phase 3: User Story 1 (P1) — Ci/Oi default-fill 🎯 MVP

**Goal**: A JIT-compiled shader that never assigns `Ci`/`Oi` renders using
the attribute-default fallback, matching the interpreter, instead of reading
uninitialized `varying[VARIABLE_CI]`/`[VARIABLE_OI]`.

**Independent Test**: Render `examples/rib/camera-dof.rib` (or an equivalent
minimal scene) with a surface shader that never writes `Ci`/`Oi`, once
pinned to `shaderformat` `"rslo"` and once to `"slo"`; outputs must match
within the project's existing visual-regression tolerance.

### Tests for User Story 1 (write first, confirm Red before T012)

- [X] T008 [P] [US1] In
  `src/libshader/tests/test_used_parameters_gating.cpp`, add a fixture +
  assertion: a `.sl` shader that never assigns `Ci` or `Oi` and references
  no other RSL globals compiles (via the real `oshader --jit` emission path,
  in-process) to a `usedParameters` bitmask with `PARAMETER_CI`/
  `PARAMETER_OI` clear (gating-condition-contract.md row 1). Must fail
  (Red) against current `llvmEmitter.cpp`.
- [X] T009 [P] [US1] Same file, add the no-regression companion fixture: a
  shader that explicitly assigns both `Ci` and `Oi` compiles to
  `PARAMETER_CI`/`PARAMETER_OI` set (gating-condition-contract.md row 2).
  Confirm this one already passes pre-fix (it exercises the always-on
  behavior, not the bug).
- [X] T010 [P] [US1] Create
  `tests/shading_parity/test_used_parameters_oracle.cpp` (new executable +
  `add_test` block added to T001's `tests/shading_parity/CMakeLists.txt`,
  ctest name `ShadingParity_UsedParametersOracle`, label `shading_parity`):
  compile the never-writes-Ci/Oi fixture to both `.slo` (`oshader --jit`)
  and `.rslo` (`oshader`), load each via
  `CRenderer::context->getShader(...)` after `RiBegin`/`RiWorldBegin`
  (pattern from `tests/imager/test_imager_execution.cpp`), and assert
  `CShader::usedParameters` (`src/libshader/shading/shader.h:151`) is
  bit-for-bit identical between the two loads
  (differential-oracle-contract.md). Must fail (Red) pre-fix.

### Implementation for User Story 1

- [X] T011 [US1] In `src/libshader/compiler/llvmEmitter.cpp`, replace the
  current `kParamBits` global-variable scan (lines ~208-245) to scan every
  `IRInstr`'s `result` *and* `operands` for name matches against
  `kParamBits`, instead of trusting `v.slcType & SLC_GLOBAL` on the
  pre-seeded `ir.vars` list — include `result` so write-only references
  still set the bit (matches interpreter semantics at `rslo.y:487-490`).
  This is half of the single `computeUsedParameters` mechanism; do not wire
  in the opcode/function half yet (see T013). (depends on T006)
- [X] T012 [US1] Run T008-T010; confirm they now pass (Green). If T010
  still fails, the failure must be isolated to a construct T013
  (opcode/function half, User Story 2) is responsible for — not Ci/Oi.
- [X] T032 [US1] Add a new `-L visual` fixture (RIB scene + surface shader
  that never writes `Ci`/`Oi`, pinned to `shaderformat` `"slo"`, with a
  sibling scene pinned to `"rslo"`, and a checked-in reference image) under
  the existing `tests/visual/` convention. This is SC-001's durable
  automated regression guard: T010's oracle only checks the
  `usedParameters` bitmask, not that the runtime's default-fill fallback
  actually produces matching pixels, and quickstart.md Step 1's before/after
  comparison is a one-time manual walkthrough, not a permanent ctest asset.
  (Numbered T032, out of strict sequence with the rest of this phase, to
  avoid renumbering every downstream task ID. No other task depends on
  this one.)

**Checkpoint**: User Story 1 fully functional and independently verifiable
via `ctest -L libshader` + `ctest -L shading_parity` + `ctest -L visual`,
filtered to Ci/Oi-related fixtures.

---

## Phase 4: User Story 2 (P1) — usedParameters bit parity for raytrace / message-passing / non-ambient / derivative

**Goal**: The JIT's `usedParameters` bitmask matches the interpreter's for
every construct gated by `PARAMETER_RAYTRACE`, `PARAMETER_MESSAGEPASSING`,
`PARAMETER_NONAMBIENT`, and the derivative-footprint family — not just
Ci/Oi.

**Independent Test**: Per spec.md Acceptance Scenarios 1-4 (trace/gather
call, displacement-shader `surface()` call, illuminance-only vs.
illuminate/solar call, `texture()` call with no literal `du`/`dv` token) —
each construct's JIT-computed bit must match the interpreter's.

**Coupling note**: research.md D2 mandates ONE combined scan mechanism
(not two separate fixes), specifically because a variable-name-only fix
would silently regress the derivative bits for shaders that call
`texture()`/`environment()` without ever mentioning `du`/`dv` — turning
today's always-on bug into a silent one. T016 below is the second half of
the same task T011 started; both must land together before either story's
oracle tests are expected to stay Green.

### Tests for User Story 2 (write first, confirm Red)

- [X] T013 [P] [US2] In `src/libshader/tests/test_used_parameters_gating.cpp`,
  add: (a) a shader calling `trace()` (or `gather`/`occlusion`/
  `visibility`/`transmission`/`indirectdiffuse`) → `PARAMETER_RAYTRACE` set;
  (b) a displacement shader calling `surface()` (or `displacement`/
  `lightsource`/`incident`/`opposite`) → `PARAMETER_MESSAGEPASSING` set;
  (c) a shader calling only `illuminance()` → `PARAMETER_NONAMBIENT` clear;
  (d) a shader calling `illuminate()`/`solar()` → `PARAMETER_NONAMBIENT`
  set (gating-condition-contract.md rows 3-6). Must fail (Red).
- [X] T014 [P] [US2] Same file, add the regression-sensitive case: a shader
  calling `texture()` with no literal `du`/`dv` token anywhere in source →
  derivative-family bits (`PARAMETER_DERIVATIVE`/`DU`/`DV`/`DPDU`/`DPDV`)
  still set (gating-condition-contract.md row 7, Story 2 AS4). This is the
  case a variable-name-only fix would incorrectly clear.
- [X] T015 [P] [US2] In `tests/shading_parity/test_used_parameters_oracle.cpp`,
  add fixtures for raytrace, message-passing, non-ambient, and
  derivative-via-builtin, each compiled to both backends and compared
  bit-for-bit (differential-oracle-contract.md; satisfies SC-002 directly).

### Implementation for User Story 2

- [X] T016 [US2] In `src/libshader/compiler/llvmEmitter.cpp`, wire T006's
  opcode/function table into the same `computeUsedParameters` scan T011
  started: for each `IRInstr` whose opcode/callee name matches the table,
  OR in the corresponding bit(s). This single mechanism must produce
  `PARAMETER_RAYTRACE`/`PARAMETER_MESSAGEPASSING`/correct
  `PARAMETER_NONAMBIENT`/derivative-family bits, subsuming FR-002 through
  FR-005. (depends on T006, T011)
- [X] T017 [US2] Delete the now-superseded hand-written `hasNonAmbientOp`
  block (current lines ~248-259) — T016's general opcode-table scan
  naturally excludes `illuminance`/`endilluminance` (they carry `params=0`
  in `shaderOpcodes.h`), making the narrow 3-opcode special case dead code.
- [X] T018 [US2] Run T013-T015 and T008-T010 together; confirm all pass
  (Green). Confirm no fixture from either story regressed the other.

**Checkpoint**: User Stories 1 AND 2 both pass. `ctest -L libshader` and
`ctest -L shading_parity` green for all Ci/Oi + raytrace + message-passing +
non-ambient + derivative fixtures.

---

## Phase 5: User Story 3 (P2) — ambient Cl crash-safety + no double-count

**Goal**: The interpreter's ambient-`Cl` accumulation is null-safe like the
JIT's already is, and `prepareAmbient()` no longer double-accumulates each
light's `Cl`/`Ol`.

**Independent Test**: Call `RSLShading::shade()` directly with no active
ambient-light state (`alights == nullptr`) on the interpreter backend — must
not crash. Separately, render a scene using the manual ambient-drive call
site and confirm accumulated `Cl`/`Ol` reflects exactly one accumulation per
light.

### Tests for User Story 3 (write first, confirm Red)

- [X] T019 [P] [US3] Create `tests/shading_parity/test_ambient_accumulation.cpp`
  (new executable + `add_test` in `tests/shading_parity/CMakeLists.txt`,
  ctest name `ShadingParity_AmbientAccumulation`): call
  `RSLShading::shade()` (`src/libshader/shading/RSLShading.cpp:37-42`) on
  the interpreter (`.rslo`) backend with `alights == nullptr`; assert no
  crash (must reproduce the crash pre-fix, per SC-004).
- [X] T020 [P] [US3] Same file, add a double-count regression test: drive a
  scene through the manual `prepareAmbient()` call site
  (`src/libshader/shading/shading.cpp:1711-1749`) with a single ambient
  light and assert the accumulated `varying[VARIABLE_CL]`/`[VARIABLE_OL]`
  in `ss->alights->savedState[1]` reflects exactly one accumulation, not
  two.

### Implementation for User Story 3

- [X] T021 [US3] In `src/libshader/shading/execute.cpp`, add the missing
  `*alights != nullptr` guard to the interpreter's `execEnd:` block (lines
  ~669-686), copied verbatim from the already-correct JIT-mirror guard on
  the same file's lines ~507-517 (FR-007).
- [X] T022 [US3] In `src/libshader/shading/shading.cpp`, remove
  `prepareAmbient()`'s duplicate manual re-accumulation loop (lines
  ~1738-1743) — `execute()`'s own accumulation already fires for every
  light `prepareAmbient()` processes (both gated by the same
  `SHADERFLAGS_NONAMBIENT`/`PARAMETER_NONAMBIENT` condition via
  `shader.cpp:153-154`). Mirror `callAmbient()`'s already-correct pattern
  (delete, don't add a `tags[i]==0` gate) (FR-008).
- [X] T023 [US3] Run T019-T020; confirm both pass (Green).

**Checkpoint**: User Story 3 independently functional — crash-safety and
double-count both fixed, verifiable without US1/US2/US4 present.

---

## Phase 6: User Story 4 (P3) — `s_rslGlobals` redundancy + `Ol` wiring

**Goal**: Resolve the two items flagged as open questions (not confirmed
bugs) in the original investigation: whether `s_rslGlobals` in
`llvmEmitter.cpp` is genuinely redundant with `CScriptContext`'s
`addGlobalVariable()` list, and whether `Ol` (light opacity) is wired
consistently between backends.

**Independent Test**: A shader that assigns `Ol` on a light produces the
same `Ol` value/behavior whether JIT- or interpreter-compiled.

### Tests for User Story 4 (write first, confirm Red or write pending T025's determination)

- [X] T024 [P] [US4] Create `tests/shading_parity/test_ol_wiring.cpp` (new
  executable + `add_test` in `tests/shading_parity/CMakeLists.txt`, ctest
  name `ShadingParity_OlWiring`): a light shader assigning `Ol`, loaded on
  both backends, asserts consistent `Ol` set/read behavior at the same
  ambient-accumulation call sites `Cl` is handled at
  (`src/libshader/shading/shading.cpp:1711-1749` and `:1576-1623` —
  `prepareAmbient()`/`callAmbient()`); T027's audit is expected to name the
  exact `VARIABLE_OL`/`PARAMETER_OL` consumer this test should pin against
  once complete (FR-010).

### Implementation for User Story 4

- [X] T025 [US4] Investigate `s_rslGlobals`
  (`src/libshader/compiler/llvmEmitter.cpp:102-181`) against
  `CScriptContext`'s `addGlobalVariable()` list
  (`src/libshader/compiler/rslo.cpp:1120-1146`). Record a written
  determination — genuinely redundant (two hand-maintained lists of the
  same ~26 RSL globals) vs. distinct-purpose (carries type/scope info the
  seeding list doesn't) — as a new subsection in research.md (FR-009).
- [X] T026 [US4] If T025 determines `s_rslGlobals` is redundant, collapse
  it to one source of truth (derive its union-relevant data from the same
  seeding path, or vice versa) in
  `src/libshader/compiler/llvmEmitter.cpp`. If T025 determines it serves a
  distinct purpose, this task is a no-op — record that outcome in the same
  research.md subsection. (depends on T025)
- [X] T027 [US4] Audit `Ol` consumers in `src/libshader/shading/shading.cpp`
  for consistency with how `Cl` is handled at the same call sites (the two
  are treated as a pair everywhere else in the ambient-accumulation code);
  fix any inconsistency found (FR-010). (depends on T024)

**Checkpoint**: All four user stories independently functional.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T028 [P] Run `cmake --build build --config Release` then
  `cmake --install build --prefix <local-prefix>` (regenerates every
  `.rslo`/`.slo` in the deploy tree automatically — root `CMakeLists.txt`'s
  existing `install(CODE ...)` blocks, no bespoke regeneration mechanism
  needed).
- [X] T029 Run the full regression gate per quickstart.md Step 7:
  `ctest --test-dir build -L libshader --output-on-failure`,
  `ctest --test-dir build -L shading_parity --output-on-failure`,
  `ctest --test-dir build -L visual --output-on-failure` — all three must
  pass clean with no new failures (SC-005). Check `.slo` timestamps against
  source/oshader mtimes first per CLAUDE.md's deploy-tree staleness gotcha
  before attributing any `-slo` visual failure to this feature.
- [X] T030 [P] Add a one-line entry for `014-jit-shading-parity` to
  `DEVNOTES.md`'s spec-branch list, matching the existing convention (e.g.
  the `011-jit-opcode-parity` entry) (Constitution Principle VII).
- [X] T031 Walk through `quickstart.md` steps 1-6 end-to-end (Step 7 is
  T029) and confirm each documented before/after outcome actually holds.

---

## Dependencies & Execution Order

- **Setup (T001-T004)** — no dependencies, T001/T002 sequential
  (T002 needs T001's directory to exist), T003/T004 parallel with each
  other and with T001/T002 (different files).
- **Foundational (T005-T007)** — T005 and T006 sequential (T006's
  `#include`s need T005's include path); T007 depends on T006 (needs the
  table to test) and on T003 (needs the CMake slot). Blocks every user
  story phase.
- **User Story 1 (T008-T012, T032)** — tests T008-T010 depend on
  Foundational + T004/T001 CMake slots, can be written in parallel with
  each other; T011 depends on T006; T012 depends on T008-T011. T032 (new
  `-L visual` fixture, E1 remediation) depends on T011 (needs the Ci/Oi fix
  to exist so the reference image reflects correct output) and can be done
  in parallel with T012.
- **User Story 2 (T013-T018)** — tests T013-T015 can be written in
  parallel with User Story 1's tests (independent files/fixtures) but
  T016 depends on T011 (extends the same `computeUsedParameters` function)
  — **this is the one deliberate cross-story coupling in this feature**,
  per research.md D2; do not attempt to ship T011 without T016, and do not
  reorder T016 before T011.
- **User Story 3 (T019-T023)** — fully independent of US1/US2; can start
  as soon as Setup is done (T019/T020 need T001's CMake scaffold, not
  Foundational's `computeUsedParameters` work).
- **User Story 4 (T024-T027)** — T024 independent (needs only T001);
  T025 independent investigation; T026 depends on T025; T027 depends on
  T024. Fully independent of US1/US2/US3.
- **Polish (T028-T031)** — after all user stories are complete.

### Parallel example (after Setup + Foundational complete)

```
# User Story 1 and User Story 2 test-writing can proceed in parallel:
T008, T009, T010  (US1 tests)
T013, T014, T015  (US2 tests)

# User Story 3 and User Story 4 can run fully in parallel with US1/US2,
# on separate files, once Setup (T001-T004) is done:
T019, T020         (US3 tests)
T024               (US4 test)
T025               (US4 investigation, no file conflict)
```

## Implementation Strategy

**MVP first**: Complete Phase 1 (Setup) → Phase 2 (Foundational) →
Phase 3 (User Story 1) → Phase 4 (User Story 2) together, since T011/T016
are two halves of one function and US2's oracle tests won't stay green
without both. This closes the user-witnessed garbage/black-render bug
(FR-001) and the structurally-missing raytrace/message-passing bits
(FR-002/FR-003) in one pass — stop here for the smallest shippable
increment.

**Incremental delivery**: US3 (crash-safety) and US4 (`s_rslGlobals`/`Ol`)
are independent of US1/US2 and of each other — either can be picked up
next in isolation, in priority order (US3 before US4) or in parallel by a
second contributor.

## Notes

- [P] tasks touch different files and have no completed-task dependency
  within their group.
- Every gating-condition and differential-oracle test must be shown to
  fail (Red) against pre-fix code before its corresponding implementation
  task lands (Constitution Principle III) — T007 and T032 are the two
  exceptions: T007 per table-parity-contract.md's Non-goals (that tier
  structurally cannot see the bug this feature fixes); T032 because a
  `-L visual` fixture's checked-in reference image is inherently captured
  once correct output exists to capture — the same way every other
  visual-regression fixture in the existing suite is established, not a
  new exception pattern this feature introduces.
- Commit after each checkpoint, not after each task.
- T032 is numbered out of sequence (after T031) to avoid renumbering every
  task ID that follows it; it belongs to Phase 3 (User Story 1) by
  dependency and checkpoint membership, not by ID order.
