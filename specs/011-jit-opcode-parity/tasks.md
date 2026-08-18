---

description: "Task list for LLVM JIT Opcode-Coverage Parity Sweep"
---

# Tasks: LLVM JIT Opcode-Coverage Parity Sweep

**Input**: Design documents from `/specs/011-jit-opcode-parity/`

**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/`, `quickstart.md` — all present and complete.

**Tests**: Explicitly requested by this feature (FR-006 coverage guard, Constitution Principle III TDD requirement) — test/repro tasks are included and sequenced Red-before-Green.

**Organization**: Tasks are grouped by user story (spec.md, US1-US5). Per user direction, checkpoints exist both at story boundaries AND at high-risk sub-milestones inside stories (the D2 colorspace relocation, the kHandledOpcodes table migration, and gather()'s CFG scaffolding). **Execution must stop at every checkpoint below and wait for explicit user confirmation before continuing to the next block** — this is a binding instruction for whoever executes this task list (including `/speckit-implement`), not just a suggestion.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1-US5)
- File paths are exact where the source location is already confirmed (plan.md/research.md/data-model.md); tasks that require locating a not-yet-identified macro target (e.g. `MFROMEXPR`'s expansion) say so explicitly — this is a Phase-1-implementation detail per `research.md` D1, not an unresolved planning question.

## Path Conventions

Single C++ project. `src/common/`, `src/ri/`, `src/libshader/{compiler,shading}/`, `tests/{libshader,visual}/` at repository root — see `plan.md`'s Source Code tree for the full annotated layout.

---

## Phase 1: Setup

**Purpose**: Establish a known-good baseline before any change.

- [X] T001 Verify branch `011-jit-opcode-parity` is checked out and `cmake --build build --config Release` succeeds cleanly — no code changes yet.

  **Note**: The plain `cmake --build build --config Release` target hits a pre-existing, feature-unrelated failure: the Swift `orender-fb-macos` macOS framebuffer GUI target (`src/framebuffer/orender-fb-macos/`) segfaults the Swift compiler with a duplicate-module-cache-path collision (`/Users/juvenal/Projects/...` vs `/Volumes/Projects/...` — the same `.pcm` reached via two different absolute paths for this machine's mount/symlink setup). Reproduced twice identically (deterministic, not a parallel-build race). This target is unrelated to `libshader`/JIT/`ri` — nothing in this feature touches Swift or the framebuffer driver. Baseline verification instead built the exact target set this feature depends on: `orender oshader libshader_compiler libshader_runtime libshader_shading test_libshader_compiler test_visual_render` — all succeeded cleanly with zero errors.
- [X] T002 [P] Record baseline test status: run `ctest --test-dir build -L libshader --output-on-failure` and `ctest --test-dir build -L visual -E slow --output-on-failure`; save pass/fail counts as the reference point for detecting regressions this feature introduces (distinct from pre-existing failures like stale `.slo` deploy-tree artifacts).

  **Baseline recorded**: `libshader` — 1/1 passed (`LibShader_Compiler`). `visual -E slow` — 75/75 passed. Zero pre-existing failures. This is the reference point for T006 and all subsequent regression checks.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The `kHandledOpcodes[]` single-source-of-truth table (D3) that both `emitFunction()` and the Story 4 coverage-guard test consult. This blocks every user story: the `op-wrapper-abi.md` contract requires every new opcode case (Story 1 and Story 3's) to be table-keyed, and the coverage-guard's correctness requires *every already-handled* opcode to be in the table too — otherwise the guard would report currently-working opcodes as "missing" just because they're still dispatched via the old `if/else-if` chain.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete and its checkpoint is confirmed.

- [X] T003 Add a `static const char* const kHandledOpcodes[]` table in `src/libshader/compiler/llvmEmitter.cpp` (or a new sibling header if cleaner), seeded with every opcode mnemonic the current `if/else-if` chain in `emitFunction()` already recognizes (~30+ entries, trimmed of `opcodes.cpp`'s string-padding).

  **Note**: Implemented with `extern` linkage (`kHandledOpcodes` declared in `llvmEmitter.h`, defined in `llvmEmitter.cpp`), deviating from the literal `static` keyword in this task's wording — tasks.md itself hedges with "(or a new sibling header if cleaner)", and `research.md` D3 / `contracts/coverage-guard-contract.md` both require the later Story-4 ctest to "include/link" and "read [the table] directly, not re-parsed from source text," which a TU-local `static` array cannot satisfy across translation units. Seeded with all 124 distinct mnemonics confirmed via direct `grep -on 'op == "[a-zA-Z0-9_]*"'` extraction over `emitFunction()`'s full dispatch range (verified as an exhaustive superset by re-scanning the same range for any non-literal comparison form — `op.compare`/`starts_with`/etc. — none found; every branch condition is a plain `op == "literal"` disjunction). `nullptr`-terminated, matching the existing `noResultOpcodes[]` idiom in `irBuilder.cpp`. Includes `ctransform` with an inline comment noting it is currently misrouted to `op_pfrom` — a correctness bug tracked for Phase 5, not a coverage gap (`coverage-guard-contract.md`'s Non-goals: the guard checks presence, not correctness).

  **Post-review verification**: the initial superset scan was re-run unbounded (no line-range clipping) and cross-checked by direct set diff rather than inference: `grep -oE 'op == "[A-Za-z0-9_]*"'` over the *entire* file, sorted/uniqued, diffed via `comm` against the mnemonics extracted straight out of the `kHandledOpcodes[]` array definition. Both `comm -23` (in dispatch chain, missing from table) and `comm -13` (in table, not in dispatch chain) are empty — 124/124 exact match, closing the "is this genuinely a superset" question with a diff instead of an argument.

- [X] T004 Refactor `emitFunction()`'s dispatch in `src/libshader/compiler/llvmEmitter.cpp` so every existing case is keyed off `kHandledOpcodes` instead of bare string-literal `if (op == "...")` comparisons. Zero behavior change — this is a pure structural refactor (Constitution Principle I), not a fix.

  **Note**: Implemented as a single early-exit gate (`if (!isHandledOpcode(op)) continue;`) inserted immediately after the per-instruction `dst`/`dstStride` resolution and before the first `if (op == "if")` case — not a rewrite of the 124 unique inline codegen bodies into a function-pointer/switch table, which would be a large, high-risk behavioral rewrite this task's "zero behavior change" constraint rules out. Placement after `dst` resolution (rather than before it) keeps `.slo` output byte-identical for every currently-unhandled opcode, since `dst` was already resolved unconditionally for every instruction before this change. The gate makes the chain genuinely consult the table: a future `else if (op == "newop")` case added without a matching `kHandledOpcodes` entry now fails loudly (skipped before reaching the new code) instead of silently drifting, which is the actual coupling "keyed off `kHandledOpcodes`" is meant to enforce.

- [X] T005 Resolve the `gatherHeader`/`gatherhdr` lowercase/mixed-case mismatch in `src/libshader/compiler/irBuilder.cpp:261` as part of this refactor pass — this determines whether `gather()`-family opcodes can even be compared against the table correctly, and feeds directly into Phase 3 (US2) triage accuracy.

  **Note**: This was three distinct fixes, not one case-fold. Confirmed via `expression.cpp:2146` (`out_printf(out, "%s (\"o=%s\") %s %s %s %s %s ", opcodeGatherHeader, ...)`, no destination token before `category`) that `gatherHeader` is a genuine no-result construct, and via `giOpcodes.h:112,145,178` that the real emitted mnemonics are `gather` (already correct), `gatherElse`, and `gatherEnd` (capital E in both — `irBuilder.cpp` had lowercase `gatherelse`/`gatherend`) plus `gatherHeader` itself (`giFunctions.h:473`), which `irBuilder.cpp` mismatched entirely as `gatherhdr` — a different word, not a case variant. Changed the `noResultOpcodes[]` entries from `"gatherhdr", "gather", "gatherelse", "gatherend"` to `"gatherHeader", "gather", "gatherElse", "gatherEnd"`.

  **Post-review verification**: this flips `hasResult` for `gatherHeader` from `true` to `false`, which shifts `IRInstr::result` → `IRInstr::operands[0]` for every `gatherHeader` instance `irBuilder.cpp` parses — a side effect worth confirming doesn't leak into the `.rslo` interpreter path, since `irBuilder.cpp` is shared IR-construction code. `grep -rn 'gatherHeader\|GatherHeader' src/libshader/ src/ri/` shows the only consumers besides this array are: `giFunctions.h:473`'s `GATHERHEADEREXPR_*` macros (the interpreter parses the emitted text stream directly via its own opcode dispatch, never through `IRInstr::result`), `opcodes.cpp`'s string constant, and `expression.cpp:2146`'s text emission. No `.rslo` consumer reads `IRInstr::result` for the gather family, so this fix is confirmed inert on the interpreter path — its only live consumer today is `kHandledOpcodes`-gated dispatch, and `gather`/`gatherHeader`/`gatherElse`/`gatherEnd` are deliberately *not* in that table yet (Phase 3 scope), so it has zero effect until then.

- [X] T006 Build and run `ctest --test-dir build -L libshader --output-on-failure` and `ctest --test-dir build -L visual -E slow --output-on-failure`; confirm identical results to the T002 baseline (zero regression from the table refactor alone).

  **Result**: Built the same scoped target set as T001 (`orender oshader libshader_compiler libshader_runtime libshader_shading test_libshader_compiler test_visual_render`) — zero errors. `ctest -L libshader`: 1/1 passed, matching baseline. `ctest -L visual -E slow`: 75/75 passed, matching baseline. Zero regression from the T003-T005 changes.

  **Post-review verification**: per the repo's own deploy-tree gotcha (CLAUDE.md), `.slo` visual tests consume prebuilt shaders under `openrender/shaders/` that `cmake --build` does not refresh — so 75/75 green alone doesn't prove the new gate ever executed against a live compile. Closed this gap directly: `build/src/oshader/oshader --jit -Ishaders/includes -o /tmp/gate_test.slo shaders/somewood.sl` succeeded (exit 0, fresh 12608-byte `.slo` timestamped after the T006 build) and `build/src/sloinfo/sloinfo` on the output shows a complete, well-formed parameter table — confirming the gate ran against a real, just-compiled instruction stream, not stale artifacts.

**Checkpoint (Foundational)**: `kHandledOpcodes[]` exists and drives every existing dispatch case; build and full existing test suite behave identically to baseline. **STOP — wait for user confirmation before any new-opcode work begins.**

---

## Phase 3: User Story 2 - Reachability inventory (Priority: P1)

**Goal**: Produce the verified reachable-opcode list that Story 3's fix scope and Story 4's coverage-guard expected set both depend on.

**Independent Test**: A list of confirmed-reachable, currently-unhandled constructs, each backed by a minimal demonstrating shader, reviewable independently of any fix being implemented yet.

- [X] T007 [P] [US2] Write minimal `.sl` triage repros for matrix-arithmetic candidates (`mulmm, addmm, submm, divmm, negm, movemm, mfromf, mfromv, ...`) in `tests/libshader/triage/matrix_ops.sl`; compile with `oshader` (non-JIT, IR text output) and grep for each literal mnemonic to confirm frontend emission.
- [X] T008 [P] [US2] Write minimal `.sl` triage repro for `gather()`/`gatherElse`/`gatherEnd` in `tests/libshader/triage/gather.sl`; compile (post-T005 fix) and inspect IR/compiler output to determine the actual failure mode (silent-wrong-output vs. hard compile/render failure) per spec.md's Edge Cases.
- [X] T009 [P] [US2] Write minimal `.sl` triage repros for comparison/logic candidates (`veql, vneql, meql, mneql, fegt, not, xor, nxor, ...`) in `tests/libshader/triage/comparison_logic.sl`; compile and grep to confirm emission.
- [X] T010 [P] [US2] Write minimal `.sl` triage repros for array move-op candidates (`ffroma, vfroma, mfroma, sfroma, ftoa, vtoa, ...`, including uniform `u*froma` variants — spec.md's "array element access", US3 Acceptance Scenario 4) in `tests/libshader/triage/array_ops.sl`; compile and grep to confirm emission.
- [X] T011 [US2] Consolidate T007-T010 results into a Reachability Inventory table (new `specs/011-jit-opcode-parity/triage-results.md`), classifying every original ~48 candidate as reachable (with its repro reference) or not-reachable (with the specific reason: string-padding artifact, dead grammar path, etc.) — per `data-model.md`'s Reachability Inventory entity.

**Checkpoint (US2 — mandatory review gate)**: Present the confirmed-reachable-vs-not opcode list to the user for review. **STOP — do not proceed to Phase 4 or Phase 6/7 until the user confirms the inventory is accurate.**

---

## Phase 4: User Story 4, part 1 of 2 - Coverage-guard test authoring (Priority: P2, sequenced early for TDD)

**Goal**: Get the dynamic coverage-guard test (FR-006) written and confirmed RED before any Story 1/3 fix lands — required by Constitution Principle III (TDD, NON-NEGOTIABLE) and the plan's explicit TDD sequencing note. Positioned here (ahead of Story 1 in document order, despite Story 1 being P1) because it structurally depends on Phase 3/US2's confirmed-reachable list and must exist in a failing state before the fixes it will later confirm.

**Independent Test**: Deliberately introduce a locally uncommitted reachable-but-unhandled construct and confirm the suite fails, naming it — but at this point in sequencing, the test is exercised via its *expected* Red state against the real, still-unfixed `cfrom`/`mfrom`/`ctransform`/Story-3 gaps, not a deliberately-injected one (that comes later, in Phase 8/T043).

- [X] T012 [US4] Add a new `libshader`-labeled ctest target (e.g. `tests/libshader/test_opcode_coverage.cpp`) that reads `kHandledOpcodes` directly (no source-text parsing) and compares it against the US2-confirmed reachable set (T011), per `contracts/coverage-guard-contract.md`'s pass/fail contract and named-mnemonic failure-message requirement.
- [X] T013 [US4] Register the new test in the relevant `tests/libshader/CMakeLists.txt` with the `libshader` ctest label; build and run it — confirm it currently **fails**, naming `cfrom`, `mfrom`, `ctransform`, and every US2-confirmed-reachable Story-3 opcode as missing (TDD Red phase).

  **Revision (post-checkpoint, still Phase 4 scope — not reopening the US2/Phase 3 review gate)**: A full accounting of `opcodes.cpp`'s 95 canonical mnemonics against `kHandledOpcodes` (124 entries) and `triage-results.md`'s 11 confirmed-dead opcodes surfaced 40 opcodes `triage-results.md` never examined — 33 already covered by Phase 6/7's planned scope, but 7 genuinely new: `vumatrix`, `vustring`, `movess` (confirmed reachable — real call sites, `expression.cpp:86,88,134,136,181,183,239,241` and `:729,785,2752`) and `moveaff`, `moveavv`, `moveass`, `moveamm` (confirmed dead — zero call sites anywhere in `src/libshader/compiler/`). Per user decision, `triage-results.md` stays frozen/unreopened; instead: (1) the test's hand-maintained `kReachableOpcodes[]` static list is replaced with a **computed set** — a new `kAllOpcodeMnemonics[]` array (`opcodes.h`/`.cpp`, all 95 mnemonics, stripped of `.rslo`-format padding, `nullptr`-terminated) minus a new `kDeadOpcodes[]` array (`opcodes.h`/`.cpp`, the original 11 dead opcodes from `triage-results.md` plus these 4 new ones, each with an inline evidence comment) — computed at test-run time in `test_opcode_coverage.cpp`, so no future opcode addition to `opcodes.cpp` can silently fall outside the guard's accounting the way the original static-list design could; (2) the 3 newly-found reachable opcodes get real task coverage — see new T029a below — rather than being silently dropped.

  **Revision implemented and re-verified**: `kAllOpcodeMnemonics[]`/`kDeadOpcodes[]` (15 entries) added to `opcodes.h`/`.cpp`, `stripOpcodeMnemonic()` helper added, `test_opcode_coverage.cpp` rewritten to compute the reachable set at test-run time, `CMakeLists.txt`'s comment updated, `research.md`'s D3 section rewritten to describe the computed-set design (with a revision-note subsection preserving the old design's rationale for history). Rebuilt and re-ran `ctest --test-dir build -L libshader --output-on-failure`: RED as expected — 44 passed, 36 failed, 80 opcodes tested (95 mnemonics − 15 dead, exactly as designed). The failure list now includes `movess`, `vumatrix`, `vustring` alongside all previously-known gaps (`cfrom`/`mfrom`, matrix arithmetic, `gather()` family, vector comparisons, array move ops) — confirming the 3 newly-found reachable opcodes are no longer silently dropped from the guard's accounting. `ctransform` and the scalar comparisons `felt`/`flt`/`fgt` are absent from the failure list (already present in `kHandledOpcodes`, unrelated to this revision) — `ctransform`'s silent-wrong-output bug (misrouted to `op_pfrom`) is a Phase 5 fix, not a coverage gap this test is designed to catch.

**Checkpoint (US4 part 1)**: Coverage-guard test exists, runs under `ctest -L libshader`, and is confirmed RED with the expected missing-opcode names. **STOP — wait for user confirmation before Phase 5 (Story 1 implementation) begins.**

---

## Phase 5: User Story 1 - `cfrom`/`mfrom`/`ctransform` parity fix (Priority: P1) 🎯 MVP

**Goal**: JIT output for the explicit-colorspace color constructor, the explicit-space matrix constructor, and `ctransform()` matches the interpreter backend.

**Independent Test**: Render a bare-sphere scene with a diagnostic shader once pinned to `rslo`, once to `slo`; before the fix the renders diverge, after they match within visual-regression tolerance.

### Sub-milestone: D2 relocation (colorspace functions out of `ri`)

- [X] T014 [US1] Create `src/common/colorSpace.h` declaring `convertColorFrom(float*, const float*, ECoordinateSystem)` and `convertColorTo(float*, const float*, ECoordinateSystem)`, and `src/common/colorSpace.cpp` with the function bodies relocated **verbatim** from `src/ri/init.cpp:67` and `:228` (no logic changes — FR-009).
- [X] T015 [US1] Add `colorSpace.h`/`colorSpace.cpp` to `src/common/CMakeLists.txt`'s `openrendercommon` sources, alongside the existing `rslConstants.cpp` precedent.
- [X] T016 [US1] Remove the `convertColorFrom`/`convertColorTo` definitions from `src/ri/init.cpp`; add the `colorSpace.h` include so `ri`'s own callers still resolve. (Included as `"common/colorSpace.h"` — `src/` is a global include root added before `common`'s `add_subdirectory()`, so headers under `src/common/` require the `common/` prefix from outside that directory; confirmed against existing precedent in `src/ri/*.cpp`.)
- [X] T017 [US1] Update `src/libshader/shading/execute.cpp:53` to replace the raw `extern` declarations with `#include "colorSpace.h"`. (Same `common/colorSpace.h` prefix rule as T016.)
- [X] T018 [US1] Update `src/libshader/shading/shaderOpcodes.h:504`'s `CFROMEXPR` macro and `src/libshader/shading/shaderFunctions.h:514,536-537`'s `CTRANSFORMEXPR`-adjacent macros to resolve via the relocated `colorSpace.h` declarations — same call, new header, no behavior change. (No edits needed: both macros reference `convertColorFrom`/`convertColorTo` unqualified, and both consuming files' `#include "common/colorSpace.h"` (T016/T017) already precede the `#include "scriptOpcodes.h"`/`"scriptFunctions.h"` lines that expand these macros inside the interpreter's `switch(opcode)` — declarations are in scope by expansion time.)
- [X] T019 [US1] Build; run `ctest --test-dir build -L libshader --output-on-failure` and `ctest --test-dir build -L visual -E slow --output-on-failure` (only interpreter/`rslo` code paths touched so far); confirm zero behavioral change vs. the T002/T006 baseline. (Build green aside from a pre-existing, unrelated Swift module-cache crash in the macOS preview/framebuffer targets. `libshader` suite: 44 passed/36 failed — byte-identical failure set to a `git stash`-verified baseline run, confirming zero regression. Visual suite: 75/75 passed, 100%.)

**Checkpoint (US1 sub-milestone — relocation)**: `convertColorFrom`/`convertColorTo` now live in `src/common/`; `ri` and the interpreter both compile and behave identically to baseline. **STOP — wait for user confirmation before adding any JIT-side wrapper code.**

### Sub-milestone: JIT wrappers + emitter cases

- [X] T019a [US1] Add `add_visual_test(<name>-slo, ...)` case(s) to `tests/visual/CMakeLists.txt` (pattern ~line 247-287) for `cfrom`/`ctransform`, using the `rslo` render as the golden/reference image. Build and run `ctest --test-dir build -L visual --output-on-failure`; confirm these new cases currently **FAIL** (TDD Red — `op_cfrom`/`op_ctransform` don't exist yet). (`sphere-cfrom-reyes-slo`/`sphere-ctransform-reyes-slo` added and confirmed Red: 140 and 127 block-diff failures respectively vs. threshold 20. **`mfrom` visual coverage deferred**: no shader can expose an `mfrom`-constructed matrix's value without also exercising matrix arithmetic/`transform()`, which the JIT doesn't handle until Phase 3 — a fixture today couldn't isolate an `op_mfrom` failure from a `mulmm`/`transform` one. `mfrom`'s wrapper still lands at T021; its parity is guarded by the T012/T013 `LibShader_OpcodeCoverage` test in the interim, and a dedicated visual case gets added alongside the matrix-arithmetic phase's own fixtures. `ShaderCompilerImmutability` references established for the two new diagnostic shaders `show_st_hsv.sl`/`show_ctransform.sl`.)
- [X] T020 [US1] Add `op_cfrom(float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags)` to `src/libshader/shading/rslOps.h`/`.cpp`, cloned from `op_pfrom` (`rslOps.cpp:493`): resolve `cSystem` via `ctx->jitFindCoordinateSystem`, then call `convertColorFrom` per active tag — per `contracts/op-wrapper-abi.md`. (Landed via a shared `getColorCoordinateSystem()` static helper that wraps `jitFindCoordinateSystem`, defaulting to `COLOR_RGB` — the identity case in `convertColorFrom`/`convertColorTo` — when unresolved.)
- [X] T021 [US1] Locate `MFROMEXPR`'s target function in `shaderOpcodes.h` (Phase-1 implementation detail per `research.md` D1) and add `op_mfrom` to `rslOps.h`/`.cpp` following T020's pattern, delegating to that same function. (`MFROMEXPR` = `mulmm(res, from, op)`; `op_mfrom` resolves the raw `from` matrix via the existing `getFromMatrix()` helper — same one `op_pfrom` uses — then calls `mulmm()`, falling back to an identity copy on unresolved space, matching `op_pfrom`'s fallback convention.)
- [X] T022 [US1] Add `op_ctransform` to `rslOps.h`/`.cpp`, delegating to `convertColorTo` (confirmed distinct from `convertColorFrom` per `research.md` D1) via the same `jitFindCoordinateSystem`-based space resolution. (Shares `getColorCoordinateSystem()` with T020.)
- [X] T023 [US1] In `src/libshader/compiler/llvmEmitter.cpp`: add `cfrom`/`mfrom` entries to `kHandledOpcodes` and their `emitFunction()` cases (clone of the `pfrom` block, `llvmEmitter.cpp:942-978`, calling `declareOp(mod, "op_cfrom"/"op_mfrom", ty)` + `B.CreateCall(...)` — zero raw IR construction). (Landed as a new `else if (op == "cfrom" || op == "mfrom" || op == "ctransform")` block right after the `pfrom`/`vtransform`/`ntransform`/`transform` block, sharing its space-string-stripping + `declareOp`/`CreateCall` shape.)
- [X] T024 [US1] In `llvmEmitter.cpp`: remove `ctransform` from the `pfrom`-family condition list; add its own `kHandledOpcodes` entry + `emitFunction()` case calling `op_ctransform`. (`ctransform` removed from the `pfrom`/`vtransform`/`ntransform`/`transform` condition and the stale "pfrom, ctransform" comment on the `fnName = "op_pfrom"` fallback deleted; `ctransform` now dispatches through T023's new block to `op_ctransform`. Stale misrouting-bug comment block removed from `kHandledOpcodes`.)
- [X] T025 [US1] Verify `op_cfrom`/`op_mfrom`/`op_ctransform` each resolve at JIT bind time via the existing `DynamicLibrarySearchGenerator` (build + a minimal JIT-shader smoke run). Only add `jitSymbolRetain.cpp`-style retention if one fails to resolve (verify-on-add per `research.md` D5) — do not add it preemptively. Confirm by inspection that none of the three contain arithmetic/conditional logic beyond marshaling — only calls to `jitFindCoordinateSystem` and `convertColorFrom`/`convertColorTo` (FR-007). (Confirmed via `Visual_sphere-cfrom-reyes-slo`/`Visual_sphere-ctransform-reyes-slo` passing — a bind-time resolution failure would render garbage/crash, not produce a pixel-accurate match. No retention shim needed. Inspection: `op_cfrom`/`op_ctransform` are a space lookup + per-tag delegating call; `op_mfrom` is a space lookup + per-tag `mulmm` call or identity-copy fallback — no independent math.)
- [X] T026 [US1] Repro per `quickstart.md` step 1: render a bare untextured sphere + `show_st.sl` pinned to `rslo`, then to `slo`; confirm the images now match for the color-constructor case (previously diverged). Repeat with matrix-constructor and `ctransform()`-exercising diagnostic shaders per spec.md's three Acceptance Scenarios. (Served by the T019a `sphere-cfrom-reyes`/`sphere-ctransform-reyes` fixtures using `show_st_hsv.sl`/`show_ctransform.sl` — both now pass `rslo` vs `slo` parity. `mfrom`'s own repro remains deferred with T019a's `mfrom` visual-coverage note: no isolated fixture exists yet since it needs matrix-arithmetic/`transform()` support to expose a value; parity is guarded by `LibShader_OpcodeCoverage` in the interim.)
- [X] T028 [US1] Build; run `ctest --test-dir build -L libshader --output-on-failure` (confirm the T012/T013 coverage-guard test's `cfrom`/`mfrom`/`ctransform` entries are now GREEN) and `ctest --test-dir build -L visual --output-on-failure` (confirm the T019a case(s) now PASS). (`LibShader_OpcodeCoverage`: `cfrom`/`mfrom`/`ctransform` no longer appear among the reported gaps — 34 failures remain, all out-of-scope Phase 3+/7 opcodes, e.g. `gather`, `veql`, `mulmm`, `ffroma`. `Visual_sphere-cfrom-reyes-slo`/`Visual_sphere-ctransform-reyes-slo`: PASS, after regenerating the two diagnostic shaders' stale `.slo` bitcode via `oshader --jit` — the deploy-tree gotcha (`.slo` files aren't refreshed by a plain `cmake --build`) applied here too, since these `.slo` were originally compiled before the emitter fix landed. Full `ctest -L visual` suite run for regression-check: **100% tests passed, 0 failed out of 79** — includes the previously-slow `motion-3-reyes` test (9.00 sec, no longer a 3-minute outlier per the T012a per-stratum motion-bounds fix from a prior feature) — zero regressions across the entire existing visual-regression corpus from this segment's changes.)

**Checkpoint (US1 complete — MVP)**: `cfrom`/`mfrom`/`ctransform` produce JIT output matching the interpreter; their coverage-guard entries are GREEN; new visual-regression cases pass. **STOP — wait for user confirmation before starting Phase 6.**

---

## Phase 6: User Story 3a - Broader sweep: matrix arithmetic, comparison/logic, array move (Priority: P2)

**Goal**: Every US2-confirmed-reachable opcode in these three categories reaches JIT/interpreter parity, plus `movess`/`vumatrix`/`vustring` (T029a — found post-checkpoint via Phase 4's revised accounting, not part of the original `triage-results.md` scope, but naturally landing here). `gather()` is deliberately excluded — see Phase 7.

**Independent Test**: For each confirmed-reachable construct in scope, render a minimal shader exercising it under both backends and confirm the outputs match.

- [ ] T028a [US3] Add `add_visual_test(<name>-slo, ...)` case(s) to `tests/visual/CMakeLists.txt` for matrix arithmetic, comparison/logic, array-element-access, and T029a's data-movement opcodes confirmed reachable. Build and run `ctest --test-dir build -L visual --output-on-failure`; confirm these new cases currently **FAIL** (Red — no `op_*` wrappers exist yet for these opcodes).
- [ ] T029 [P] [US3] For each US2-confirmed-reachable matrix-arithmetic opcode: locate its interpreter macro/target function in `shaderOpcodes.h`/`shaderFunctions.h`; add a matching `op_*` wrapper to `rslOps.h`/`.cpp` delegating to that same function — no new math. Includes `movemm` (`scriptOpcodes.h:725`, `MUNARYEXPR`, 16-float per-vertex copy) — add `op_movemm` to `rslOps.h`/`.cpp` following the existing `op_movevv`/`op_moveff` copy-wrapper pattern (`rslOps.cpp:293-299`).
- [ ] T029a [P] [US3] For `movess`, `vumatrix`, `vustring` (found via the full `opcodes.cpp` accounting during Phase 4's revision, not in `triage-results.md` — see T012's revision note): add `op_movess` to `rslOps.h`/`.cpp` — a string-pointer sibling of `op_moveff`/`op_movevv` (`rslOps.cpp:293-299`), delegating to the same simple per-vertex-copy pattern the interpreter's `SUNARYEXPR` macro implements (`scriptOpcodes.h:726`). Then add `vumatrix`/`vustring` as trivial `emitFunction()` cases that reuse `op_movemm` (from T029) and `op_movess` with source stride forced to `0` — mirroring the existing `vufloat`/`vuvector` uniform-broadcast pattern already in the emitter (`llvmEmitter.cpp:883-897`), **not** new wrapper functions. Depends on T029's `op_movemm` landing first (not fully parallel with T029 despite the `[P]` marker — parallel only with T030/T031).
- [ ] T030 [P] [US3] For each US2-confirmed-reachable comparison/logic opcode: same delegation pattern as T029; check first whether an existing `op_*` can be reused with inverted/swapped args (e.g. `not`) before adding a new function.
- [ ] T031 [P] [US3] For each US2-confirmed-reachable array move-op opcode (spec.md's "array element access" family: `ffroma`/`vfroma`/`mfroma`/`sfroma`/`ftoa`/`vtoa`/etc., array-indexed reads/writes using stride math): locate the interpreter's array-indexing/stride helper in the `.rslo` path and delegate to it from a new `op_*` wrapper, rather than reimplementing array-stride math in the emitter. Scope note: this is a distinct family from T029a's `vumatrix`/`vustring` (uniform-to-varying *promotion*, not array indexing) — do not conflate the two.
- [ ] T032 [US3] Add `kHandledOpcodes` + `emitFunction()` cases in `llvmEmitter.cpp` for every wrapper added in T029-T031 and T029a, following the `declareOp`+`CreateCall` pattern (T029a's `vumatrix`/`vustring` cases reuse T029/T029a's wrappers directly, per that task's note).
- [ ] T034 [US3] Build; run `ctest --test-dir build -L libshader --output-on-failure` (confirm these opcodes' coverage-guard entries now GREEN) and `ctest --test-dir build -L visual --output-on-failure` (confirm the T028a cases now PASS). Confirm by inspection that every `op_*` wrapper added in T029-T031 and T029a contains no new arithmetic beyond marshaling (FR-007).

**Checkpoint (US3a complete)**: matrix arithmetic, comparison/logic, and array-move opcodes reach parity; their coverage-guard entries are GREEN. **STOP — wait for user confirmation before starting `gather()` (Phase 7).**

---

## Phase 7: User Story 3b - Broader sweep: `gather()` (Priority: P2, higher risk/uncertain scope)

**Goal**: Fix `gather()`/`gatherElse`/`gatherEnd` — split from Phase 6 because it needs new loop/CFG scaffolding (not a simple wrapper clone) and its failure mode/reachability is only confirmed by Phase 3/T008.

**Independent Test**: Render a shader using `gather()` for ambient occlusion or indirect illumination sampling under the JIT backend; output matches the interpreter backend.

- [ ] T035 [US3] Gate check: if Phase 3's triage (T008/T011) found `gather()` unreachable, **stop here**, record the descope decision with rationale in `research.md`/`spec.md`'s Assumptions, and skip T036-T041. Per FR-005/SC-003, "reachable but unfixed" is not a permitted outcome — if Phase 7's scope proves difficult during implementation, escalate to the user for an explicit scope decision (which would require a corresponding spec.md amendment) rather than unilaterally descoping here.
- [ ] T035a [US3] (Only if T035 did not descope.) Add `add_visual_test(gather-slo, ...)` case(s) to `tests/visual/CMakeLists.txt` covering ambient-occlusion/indirect-illumination sampling. Build and run `ctest --test-dir build -L visual --output-on-failure`; confirm it currently **FAILS** (Red — no gather scaffolding exists yet).
- [ ] T036 [US3] Study `illuminance`/`endilluminance`'s scope-tracking structure (`llvmEmitter.cpp:629,678`) as the template for lowering a looping RSL construct with a body block.
- [ ] T037 [US3] Design and implement equivalent scope-tracking scaffolding for `gather`/`gatherElse`/`gatherEnd` in `llvmEmitter.cpp`, delegating the actual GI/AO sampling to whatever function the interpreter's existing `gather` handling calls — no new sampling math.
- [ ] T038 [US3] Add the corresponding `op_*` wrapper(s) in `rslOps.h`/`.cpp` for the delegation target identified in T037.
- [ ] T039 [US3] Add `kHandledOpcodes` + `emitFunction()` entries for `gather`/`gatherElse`/`gatherEnd`.
- [ ] T041 [US3] Build; run `ctest --test-dir build -L libshader --output-on-failure` (confirm the `gather` family's coverage-guard entries GREEN) and `ctest --test-dir build -L visual --output-on-failure` (confirm the T035a case now PASSES). Confirm by inspection that the T038 wrapper(s) delegate to the interpreter's existing gather-sampling function with no new sampling math (FR-007).

**Checkpoint (US3b complete)**: `gather()` family reaches parity, or is explicitly and documentedly descoped per T035. **STOP — wait for user confirmation before Phase 8.**

---

## Phase 8: User Story 4, part 2 of 2 - Coverage-guard finalization (Priority: P2)

**Goal**: Confirm the guard is fully green against real state and genuinely dynamic (catches future gaps, not just this feature's original inventory).

**Independent Test**: Deliberately introduce (locally, uncommitted) a new reachable construct with no JIT handling — including one outside this feature's original inventory — and confirm the suite fails, naming it; then confirm it passes cleanly with the change reverted.

- [ ] T042 [US4] Build; run `ctest --test-dir build -L libshader --output-on-failure`; confirm the coverage-guard test now passes 100% (every US2-confirmed-reachable opcode present in `kHandledOpcodes`, adjusted for any T035 descope).
- [ ] T043 [US4] Per spec.md Acceptance Scenario 3: locally add one new reachable-but-unhandled construct (or temporarily remove one `kHandledOpcodes` entry); confirm the test fails, naming that specific construct; then revert the local change and confirm the suite is green again.

**Checkpoint (US4 complete)**: Coverage-guard test passes against real state and is confirmed to catch a deliberately-introduced gap it wasn't specifically written for. **STOP — wait for user confirmation before Phase 9.**

---

## Phase 9: User Story 5 - Converge JIT-only helpers onto delegation (Priority: P3)

**Goal**: `jitArea`/`jitCalculateNormal`/`jitDepth`/`jitDuVector`/`jitDvVector` (`shading.cpp`) stop being hand-reimplemented duplicates and call the same function the interpreter uses, like `op_specular_batch` already does.

**Independent Test**: Render a shader using `area()`, `calculatenormal()`, `depth()`, `Du()`, `Dv()` under the JIT backend before/after; output is unchanged; by inspection, only one implementation of each remains.

- [ ] T044 [P] [US5] Refactor `jitArea` (`shading.cpp`) to call the same function/macro body `AREAEXPR` (`shaderFunctions.h`) expands to, rather than its hand-reimplemented copy.
- [ ] T045 [P] [US5] Refactor `jitCalculateNormal` (`shading.cpp`) to delegate to the interpreter's calculate-normal implementation.
- [ ] T046 [P] [US5] Refactor `jitDepth` (`shading.cpp`) to delegate to the interpreter's depth implementation.
- [ ] T047 [P] [US5] Refactor `jitDuVector`/`jitDvVector` (`shading.cpp`) to delegate to the interpreter's `Du`/`Dv` implementations.
- [ ] T048 [US5] Build; render a shader exercising all five constructs under the JIT backend before/after T044-T047; confirm output is byte-identical.
- [ ] T049 [US5] Run `ctest --test-dir build -L visual --output-on-failure`; confirm zero regressions.

**Checkpoint (US5 complete)**: all five helpers delegate to the interpreter's implementation; visual output unchanged. **STOP — wait for user confirmation before Phase 10 (Polish).**

---

## Phase 10: Polish & Cross-Cutting Concerns

**Purpose**: FR-011 performance verification and FR-010 doc corrections — deferred to last since they depend on every construct fixed in Phases 5-7 existing.

- [ ] T050 [P] Register the FR-011 manual-only performance-comparison `ctest` assertion (label `perf-manual`, exact name per `research.md` D4) per `quickstart.md` step 4; confirm it is excluded from default/CI `ctest` invocations.
- [ ] T051 [P] Manually run the `perf-manual` benchmark for each construct fixed under Phases 5-7 (`cfrom`, `mfrom`, `ctransform`, matrix arithmetic, comparison/logic, array move, `gather` if not descoped); confirm JIT time ≤ 90% of interpreter time per FR-011/SC-006; record results.
- [ ] T052 [P] Update `DEVNOTES_DETAILS/BUGS.md`: extend the `cfrom` entry to cover `mfrom`/`ctransform`; move to Resolved.
- [ ] T053 [P] Correct `DEVNOTES_DETAILS/OSHADER_UPDATES.md` and `CLAUDE.md` gotcha #3: remove the false "unrecognised opcode warning" claim; remove/adjust the `jitSymbolRetain.cpp` reference unless T025/T038 ended up genuinely needing it.
- [ ] T054 Run the full `quickstart.md` validation guide end-to-end as a final sanity check (all 5 steps).
- [ ] T055 Run `ctest --test-dir build -L visual --output-on-failure` (full suite, including `motion-3-reyes`) and `ctest --test-dir build -L libshader --output-on-failure` one final time; confirm everything green. Do a final FR-007 sweep: every `op_*` function added by this feature (T020-T022, T029-T031, T038) contains only marshaling code, confirmed by cross-referencing each against its `RSL Construct.interpreter_target` in `data-model.md`.

**Final checkpoint**: Feature complete. **STOP — wait for user confirmation before considering this feature done and before any commit.**

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies.
- **Foundational (Phase 2)**: Depends on Setup — BLOCKS all user stories.
- **US2 / Phase 3**: Depends on Foundational (needs T005's `gatherHeader` fix for accurate `gather` triage).
- **US4 part 1 / Phase 4**: Depends on Foundational (needs `kHandledOpcodes`) AND US2/Phase 3 (needs the confirmed-reachable set) — this is why it is sequenced before US1 in this document despite US1 being P1: the coverage-guard test must exist and be RED before Phase 5's fixes land (Constitution Principle III).
- **US1 / Phase 5**: Depends on Foundational + US4-part-1 (Red state established). Independently testable/shippable once its own checkpoint passes — does not require US3 or US5.
- **US3a / Phase 6** and **US3b / Phase 7**: Depend on Foundational + US2 (confirmed-reachable set). Independent of US1. US3b depends on US3a only in document ordering, not technically — it could run first or in parallel with different staffing, but is sequenced after US3a here per the user's split (simpler/lower-risk work checkpointed before the higher-risk `gather()` scaffolding).
- **US4 part 2 / Phase 8**: Depends on US1 (Phase 5) + US3a (Phase 6) + US3b (Phase 7, or its T035 descope) all being complete — this is where the guard's 100%-green state is confirmed.
- **US5 / Phase 9**: Depends only on Foundational. Not dependent on US1/US3/US4 content-wise, but sequenced last (P3, lowest priority, pure risk-reduction with no currently-known bug) per spec.md's priority ordering.
- **Polish (Phase 10)**: Depends on all prior phases (perf benchmarks need every fixed construct; doc fixes reference the merged bug entries).

### Parallel Opportunities

- T007-T010 (US2 triage repros, four different opcode categories) run in parallel.
- T029-T031 (US3a wrapper additions, three different opcode categories) run in parallel; T029a depends on T029's `op_movemm` and so is not fully parallel with it (parallel only with T030/T031).
- T044-T047 (US5 helper refactors, four different functions in the same file but independent bodies) can be developed in parallel then merged.
- T050-T053 (Polish: perf test registration, benchmark runs, and the three doc-file corrections) run in parallel.
- Once Foundational + US2 are both complete, US1 (Phase 5) and US3a/US3b (Phases 6-7) have no technical dependency on each other and could be staffed in parallel — they are presented sequentially here only because the user requested sequential checkpoints, not because of a code dependency.

---

## Parallel Example: Phase 3 (User Story 2 triage)

```bash
Task: "Write matrix-arithmetic triage repro in tests/libshader/triage/matrix_ops.sl"
Task: "Write gather() triage repro in tests/libshader/triage/gather.sl"
Task: "Write comparison/logic triage repro in tests/libshader/triage/comparison_logic.sl"
Task: "Write array move-op triage repro in tests/libshader/triage/array_ops.sl"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Phase 1 (Setup) → Phase 2 (Foundational) → Phase 3 (US2 triage, needed for T012's guard-test content) → Phase 4 (US4 part 1, Red guard) → Phase 5 (US1).
2. **STOP and VALIDATE** at Phase 5's checkpoint: `cfrom`/`mfrom`/`ctransform` parity achieved, coverage guard partially green, documented workaround (`Option "shaderformat" "default" ["rslo"]`) no longer necessary for this family.
3. This is the smallest fully-shippable increment — matches spec.md's own MVP framing for US1.

### Incremental Delivery

1. Setup + Foundational + US2 + US4-part-1 → Red-state guard ready.
2. Add US1 → validate independently → MVP.
3. Add US3a → validate independently.
4. Add US3b (`gather()`) → validate independently, or descope with documented rationale.
5. Add US4-part-2 → confirm guard fully green and dynamic.
6. Add US5 → validate independently (no behavior change expected).
7. Polish → FR-011 benchmarks + doc corrections + full final validation.

Each step adds value without breaking previous steps; every checkpoint is a valid stopping point if priorities shift.

---

## Notes

- [P] tasks touch different files/opcode categories with no dependencies between them.
- [Story] label maps each task to its spec.md user story for traceability.
- **Every checkpoint in this document is a mandatory stop** — do not proceed to the next phase or sub-milestone without explicit user confirmation, per the user's instruction for this task list.
- Tests are written before the corresponding fix where TDD sequencing applies (T012/T013's coverage-guard test before Phases 5-7's fixes; T007-T010's triage repros before Phase 3's inventory is finalized; T019a/T028a/T035a's visual-regression Red-state stubs before each phase's `op_*` implementation).
- Commit only when the user explicitly asks — this feature's standing instruction (from `/speckit-specify` through `/speckit-plan`) is no automatic commits at any step, including at checkpoints.
- Avoid: reimplementing shading math independently in any `op_*` wrapper (FR-007, non-negotiable); changing interpreter (`.rslo`) behavior (FR-009); checking the coverage guard at render runtime (FR-006).
- `kDeadOpcodes[]` (`opcodes.h`/`.cpp`, added during Phase 4's revision) is now the single source of truth for confirmed-dead opcodes — 15 entries: the original 11 from `triage-results.md` plus `moveaff`/`moveavv`/`moveass`/`moveamm` (found post-checkpoint, zero call sites in `src/libshader/compiler/`). `triage-results.md` itself is intentionally left unedited/frozen — its Phase 3 review gate already passed and is not reopened by this addition.
