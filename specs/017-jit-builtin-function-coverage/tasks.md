# Tasks: LLVM JIT Builtin-Function Coverage

**Input**: Design documents from `/specs/017-jit-builtin-function-coverage/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md (all present)

**Tests**: Included and REQUIRED — spec.md's FR-018 mandates a persisted
regression test per function, and the Constitution Check (plan.md) carries
an explicit TDD sequencing gate: every function's probe shader + RIB pair
must exist and be confirmed failing *before* that function's engine fix
lands.

**Organization**: Tasks are grouped by user story (US1–US5, matching
spec.md's priorities P1/P1/P2/P3/P3) to enable independent implementation
and testing of each story. US5 was added mid-implementation (during US2)
when actually running US2's coverage guard surfaced 18 previously-
uninventoried unhandled functions — see `spec.md`'s 2026-09-20
mid-implementation Clarifications entry and `research.md` D9.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an
  incomplete task)
- **[Story]**: Which user story this task belongs to (US1/US2/US3/US4/US5)
- Exact file paths are given in every task description

## Path Conventions (single project — see plan.md's Project Structure)

- Compiler: `src/libshader/compiler/llvmEmitter.cpp`, `.h`
- Runtime: `src/libshader/shading/rslOps.cpp`, `.h`, `shading.h`, `.cpp`
- Coverage guard: `src/libshader/tests/test_opcode_coverage.cpp`
- Probe shaders: `shaders/<name>_probe.sl`
- RIB scenes + references: `examples/rib/tests/sphere-<name>-reyes{,-slo}.rib`,
  `examples/rib/tests/references/sphere-<name>-reyes.tif`
- Test registration: `tests/visual/CMakeLists.txt`
- Build environment: vcpkg toolchain, already active on this branch
  (`research.md` D8 — no setup task needed)

---

## Phase 1: Setup

**Purpose**: Confirm the branch/environment groundwork already completed
during planning is in a known-good state before any code change.

- [X] T001 Confirm `VCPKG_ROOT`/`VCPKG_INSTALLED_DIR` are exported and
  `cmake -B build -S .`'s configure log resolves LLVM/PNG/TIFF/zlib/OpenEXR
  under `VCPKG_INSTALLED_DIR` (per `quickstart.md` prerequisites; the
  CMakeLists.txt `VCPKG_TOOLCHAIN` fix from `research.md` D8 is already
  committed — this task only re-verifies, does not re-fix).
- [X] T002 Run `cmake --build build --config Release` (throttled, `-j` a
  few below core count) followed by
  `ctest --test-dir build -L "visual|libshader|shading_parity" --output-on-failure`
  and confirm a 100% passing baseline before making any change in this
  feature — this is the known-good starting point every later phase's
  "zero regressions" check is measured against. Confirmed 200/200 (the 4
  parallel-run timeouts on `Visual_teapot-motion-raytrace`/
  `Visual_blobby-csg-difference-*` re-ran clean in isolation — contention
  artifacts, not real failures).

**Checkpoint**: Clean, fully-green baseline confirmed on the vcpkg
toolchain.

---

## Phase 2: Foundational

**Purpose**: This feature has no genuinely blocking shared infrastructure
to build — `research.md` D2 confirms the JIT's generic variable-resolution
machinery (`resolveVar`/`loadVarPtr`/`s_rslGlobals`) and `activeContext()`
delegation pattern (`op_shadow_f`'s existing split) already exist and need
zero new plumbing before any user story can start. This phase is
intentionally minimal.

- [X] T003 Read `contracts/op-wrapper-abi.md`, `contracts/gate-hardening-contract.md`,
  and `contracts/function-coverage-guard-contract.md` in full before
  starting US1 — every later task in this file assumes familiarity with
  these three contracts rather than re-explaining them per task.

**Checkpoint**: Foundation ready — User Story 1 can begin.

---

## Phase 3: User Story 1 — Shipped shaders using `visibility()`/`transmission()`/`trace()`/`occlusion()`/`indirectdiffuse()`/`comp()` render correctly under the JIT (Priority: P1) 🎯 MVP

**Goal**: Close the actual "JIT show-stopper" — the six builtin functions
real shipped shaders call, including the first-ever JIT ray-batch/
non-deterministic-lookup machinery (`research.md` D2).

**Independent Test**: Render `examples/rib/quadlight.rib`/`spherelight.rib`
with `Attribute "shade" "shaderformat" ["slo"]` and confirm they match
their `.rslo` reference within tolerance (spec.md US1 Acceptance Scenario 7).

**⚠️ TDD**: For every function below, the test task MUST be completed and
confirmed failing (mismatched/silently-wrong `.slo` output against the
`.rslo` reference) before that function's implementation task starts.

### Tests for User Story 1

- [X] T004 [P] [US1] Create `shaders/visibility_probe.sl` (calls `visibility()`
  between two points, writes result into `Ci`, no `Oi = 1;` needed per
  `research.md` D6) + `examples/rib/tests/sphere-visibility-reyes.rib` +
  `sphere-visibility-reyes-slo.rib` (both with `Option "limits" "numthreads" [1]`
  + explanatory comment, per `data-model.md`'s Regression Test Pair entity)
  + generate `examples/rib/tests/references/sphere-visibility-reyes.tif` +
  register both as `Visual_sphere-visibility-reyes{,-slo}` in
  `tests/visual/CMakeLists.txt`. Confirm the `-slo` variant currently fails
  against the reference. Confirmed: MaxBlockAvgDiff 254.67 (threshold 20),
  140/4800 blocks failing.
- [X] T005 [P] [US1] Same as T004 for `transmission()`: `shaders/transmission_probe.sl`,
  `sphere-transmission-reyes{,-slo}.rib`, reference `.tif`, `ctest`
  registration. Confirmed currently failing before T011, passing after.
- [X] T006 [P] [US1] Same as T004 for `trace()` — cover BOTH its float
  (nearest-hit) and color (reflection) forms in one probe shader:
  `shaders/trace_probe.sl`, `sphere-trace-reyes{,-slo}.rib`, reference
  `.tif`, `ctest` registration. Confirmed currently failing before T012,
  passing after (see T012's note on the float/boolean unpacking bug this
  probe surfaced).
- [X] T007 [P] [US1] Same as T004 for `occlusion()`: `shaders/occlusion_probe.sl`,
  `sphere-occlusion-reyes{,-slo}.rib`, reference `.tif`, `ctest`
  registration. Confirmed failing before T013, passing after.
  **Root cause of the earlier "always returns 0" investigation block
  (resolved, not an interpreter bug):** ray-visibility categories
  (diffuse/specular/transmission) default OFF
  (`CAttributes::CAttributes()`, `attributes.cpp:87-93`, only sets
  `PRIMARY_VISIBLE`/`DOUBLE_SIDED`/`DISPLACEMENTS`) — the occluder sphere
  needs an explicit `Attribute "visibility" "diffuse" [1]` or every AO
  hemisphere ray misses it and `occlusion()` legitimately, correctly
  returns exactly 0. Once added, the interpreter render is genuinely
  non-degenerate (490 unique RGB values). The `Attribute "irradiance"
  "filemode" [""]`-triggers-a-SIGSEGV finding stands as a real,
  pre-existing, interpreter-only bug — filed as out-of-scope for this
  feature, not fixed here (never pass an empty `filemode` value).
- [X] T008 [P] [US1] Same as T004 for `indirectdiffuse()`: `shaders/indirectdiffuse_probe.sl`,
  `sphere-indirectdiffuse-reyes{,-slo}.rib`, reference `.tif`, `ctest`
  registration. Confirmed passing after T014 — but this pair's reference
  is legitimately, exactly black: `indirectdiffuse()`'s hit-color
  contribution requires a baked global photon map
  (`attributes->globalMap != NULL` on the hit object,
  `irradianceDispatch.cpp` ~449), which a single-pass scene never
  produces. Interpreter and JIT still diff at exactly 0.0 (genuine
  bit-exact parity, not a skipped/vacuous test), but the pair is
  visually non-discriminating by design — documented in
  `tests/visual/CMakeLists.txt`'s registration comment. A real photon-bake
  two-pass scene was judged out of scope for this feature.
- [X] T009 [P] [US1] Same as T004 for `comp()` — cover BOTH the
  two-operand (vector-index) and three-operand (matrix-index) forms in one
  probe shader: `shaders/comp_probe.sl`, `sphere-comp-reyes{,-slo}.rib`,
  reference `.tif`, `ctest` registration. Confirmed failing before T015,
  passing after — and, unlike T004-T006, passed on the first implementation
  attempt (no CShadingContext state, no raytracing, matches research.md's
  "trivial" assessment).

### Implementation for User Story 1

- [X] T010 [US1] Implement `visibility()`: add `jitVisibility` declaration
  to `src/libshader/shading/shading.h` (public, alongside `jitShadowF` at
  line 501) and its body to `shading.cpp`, transcribing
  `TRANSMISSIONEXPR_PRE`/`TRANSMISSIONEXPR`/`_UPDATE`/`VISIBILITYEXPR_POST`
  byte-faithfully per `research.md` D2 (loop bound
  `currentShadingState->numRealVertices`, replicate into derivative-offset
  tail per D1; confirm the `scratch->traceParams` accessor spelling first,
  per D2's flagged lookup); add `op_visibility` trampoline to
  `src/libshader/shading/rslOps.h`/`.cpp` (`contracts/op-wrapper-abi.md`
  raytracing-tier shape); add `"visibility"` to `kHandledOpcodes[]` and a
  new `op == "visibility"` dispatch branch (resolving `P`/`D`/`du`/`dv`/`N`/`time`
  via `resolveVar`+`loadVarPtr`) in `src/libshader/compiler/llvmEmitter.cpp`.
  Confirmed T004 now passes (`Visual_sphere-visibility-reyes-slo` green).
  Implemented as a shared `jitTraceBatch` private helper (visibility/
  transmission/traceF/traceC all funnel through it, differing only in
  `probeOnly`/`isReflection`) — built and shipped alongside T011/T012.
  Note: shading.cpp's file-scope Mersenne Twister macros (`#define N 624`,
  `#define M 397`, etc., used only by `next_state()`) were never `#undef`'d
  and shadowed the RSL `N`/`M` identifiers for the rest of the file; added
  `#undef`s immediately after `next_state()` rather than renaming every
  downstream parameter, so later JIT code can use `N`/`M` normally.
- [X] T011 [US1] Implement `transmission()`: same file set and pattern as
  T010 (`jitTransmission`, `op_transmission`, `"transmission"` dispatch),
  transcribing `TRANSMISSIONEXPR_POST` instead of `VISIBILITYEXPR_POST`.
  Confirmed T005 now passes. Implementation landed alongside T010 (shared
  `jitTraceBatch`); this task's own work was writing T005's probe/RIB/
  reference and confirming green.
- [X] T012 [US1] Implement `trace()` (both forms): same file set and
  pattern as T010 (`jitTrace`, `op_trace_f`/`op_trace_c`, `"trace"`
  dispatch selecting float-vs-color by destination stride), transcribing
  `TRACE2EXPR_POST` (float/nearest-t) and `TRACEEXPR_POST` (color) — note
  `TRACEEXPR_PRE`/`TRACEEXPR`/`TRACEEXPR_UPDATE` `#define` directly to the
  `TRANSMISSIONEXPR*` macros (`research.md` D2), so the ray-batch
  construction logic itself is identical to T010/T011's, only the
  post-processing and destination type differ. Confirmed T006 now passes.
  Two real bugs found and fixed while landing T010-T012 together (both in
  the shared `jitTraceBatch`, so all four wrappers benefit):
  1. **Uniform-operand `duVector`/`dvVector` OOB read**: `duVector`/
     `dvVector` assume their source is a real per-vertex varying array
     (they index `src[+-3]` relative to the grid position). The JIT's
     uniform-collapse optimization can hand P/D in with stride 0 (a single
     broadcast 3-float value, e.g. `trace(P, normalize(vector(...)))`'s
     uniform direction operand) — calling duVector/dvVector on that reads
     out of bounds. Fixed by guarding on `sP`/`sD == 3` and zeroing the
     du/dv-of-P/D buffers directly when uniform (a uniform field's spatial
     derivative is exactly zero anyway, so this is also the mathematically
     correct answer, not just a safety guard).
  2. **`probeOnly` conflated two different unpacking semantics**: for a
     float (sd==1) destination, `visibility()`'s `probeOnly=TRUE`
     (`VISIBILITYEXPR_POST`) wants a 0/1 occlusion boolean, but `trace()`'s
     float form (`TraceF`/`TRACE2EXPR_POST`) ALSO uses `probeOnly=TRUE`
     (a pure distance query, no shading) yet wants the *raw hit distance*,
     not a boolean. The original shared ternary
     (`probeOnly ? boolean : rays->t`) silently zeroed every `trace()`
     float result whenever the ray missed (t stayed a huge finite
     sentinel, ternary picked the boolean branch and wrote 0.0f) — found
     via direct interpreter-vs-JIT instrumentation of `traceReflection`
     showing byte-identical ray setup and t/C values between paths, which
     narrowed the bug to the unpacking code rather than the raytrace
     itself. Fixed by adding a separate `wantBoolean` parameter to
     `jitTraceBatch`, independent of `probeOnly`: `true` only for
     `jitVisibility`, `false` for the other three wrappers.
- [X] T013 [US1] Implement `occlusion()`: `jitOcclusion` in
  `shading.h`/`.cpp` transcribing `IDEXPR_PRE`/`IDEXPR`/`_UPDATE`/`_POST`
  (uses `rendererGetCache`/`rendererGetTexture3d`/`rendererGetEnvironment`,
  protected `shading.h:514-519`, plus `CTexture3d::lookup`/
  `texture3Dunpack` — NOT `CTraceLocation`/`traceTransmission`, per
  `research.md` D2's occlusion/indirectdiffuse variant); `op_occlusion` in
  `rslOps.h`/`.cpp`; `"occlusion"` dispatch in `llvmEmitter.cpp`. Confirmed
  T007 now passes, bit-exact (0.0 diff) against the interpreter reference.
  Deviations from D2 found necessary during implementation: (a) the "!"-
  suffixed optional channel-binding extension is genuinely unused by the
  plain 3-argument call form (`lookup->numChannels` is 0 without it), so
  `cache->resolve()`/`channelValues`/`texture3Dunpack` are legitimately
  skipped entirely — `C[]` is read directly; (b)
  `COcclusionLookup::postBind()` (shaderPl.cpp:621-624, defaults
  `texture3dParams.coordsys` to `"world"` when empty) was missed on the
  first pass — omitting it produced an "Unknown coordinate system"
  warning and a silent identity-matrix fallback (numerically harmless for
  this test's identity camera transform, confirmed bit-exact either way,
  but wrong in general); added explicitly. `scratch->traceParams.samples`
  is set from the JIT-resolved third operand (`op3`/`numSamples`) via
  `JIT_IDX(samples, sSamples, i)[0]`, not raw indexing — the same
  uniform-stride care as T010-T012's `duVector` fix.
- [X] T014 [US1] Implement `indirectdiffuse()`: same file set and pattern
  as T013 (`jitIndirectDiffuse`, `op_indirectdiffuse`, `"indirectdiffuse"`
  dispatch). Confirmed T008 now passes, bit-exact (0.0 diff, both sides
  exactly black — see T008's note on why). Landed together with T013
  via a single shared `jitOcclusionBatch` private helper (same
  `wantOcclusion` bool-selector pattern as `jitTraceBatch`'s
  `isReflection`/`wantBoolean`).
- [X] T015 [US1] Implement `comp()`: `op_comp` (vector form) and
  `op_mcomp` (matrix form) free functions in `rslOps.cpp`/`.h` (no
  `CShadingContext`/`activeContext()` needed — pure indexed
  read, `research.md` D2's `comp` note); `"comp"` dispatch in
  `llvmEmitter.cpp` branching on operand count (2 vs 3), mirroring the
  `"noise"`/`"snoise"` f-vs-v dispatch shape (`llvmEmitter.cpp:1808-1822`).
  Confirmed T009 now passes. Reused `binOpTy`/`ternOpTy` (already-declared
  generic 2-/3-operand call shapes) directly — no new `FunctionType`
  needed. Passed cleanly on the first build/render, no bugs found.
- [X] T016 [US1] Create `examples/rib/tests/quadlight-slo.rib` and
  `spherelight-slo.rib` (copies of `examples/rib/quadlight.rib`/
  `spherelight.rib` with `Attribute "shade" "shaderformat" ["slo"]` added
  — the originals stay unchanged; per `data-model.md`'s Show-Stopper
  Closure Pair entity, no reference image exists for either yet, so
  generate the reference `.tif` from a `quadlight.rib`/`spherelight.rib`
  interpreter render first). Register `Visual_quadlight{,-slo}`/
  `Visual_spherelight{,-slo}` pairs in `tests/visual/CMakeLists.txt`
  (FR-019), both diffed against that reference; confirmed the `-slo`
  renders now match within tolerance now that T010–T015 are complete
  (all 4 tests green).
  Deviation from the original task wording, found necessary during
  execution: both scenes are stochastic (16-sample raytraced soft
  shadows via `visibility()`) and, unlike every other probe in this
  feature, can't take `Option "limits" "numthreads" [1]` themselves
  without diverging from the unchanged original `.rib` — confirmed two
  default-multithreaded interpreter renders of `quadlight.rib` differ by
  a full 255 (thread-scheduling nondeterminism, same root cause as
  issue #1's `random()` finding). Fixed by adding an optional 6th
  "extra orender CLI args" parameter to the `add_visual_test` CMake
  macro and a matching optional argv to `tests/visual/test_visual_render.cpp`
  (backward-compatible, unused by every other registered scene), and
  passing `"-t:1"` for these four tests only. Re-verified determinism
  after the fix: two `-t:1` renders of `quadlight.rib` diff at exactly
  0.0.
- [X] T017 [US1] Run
  `ctest --test-dir build -L "visual|libshader|shading_parity" --output-on-failure`
  in full; confirm zero regressions vs. T002's baseline and all of
  T004–T009's + T016's new pairs passing. Confirmed: 216/216 passing,
  zero regressions.

**Checkpoint**: User Story 1 fully functional and independently testable —
the actual "JIT show-stopper" is closed. `quadlight.rib`/`spherelight.rib`
render correctly under `--jit`. All 216 tests pass.

---

## Phase 4: User Story 2 — A missing JIT builtin function can never again ship unnoticed (Priority: P1)

**Goal**: Close the defect *class* — harden the silent-skip gate into a
build-time error, and extend the test-time coverage guard to the full
builtin-function mnemonic set.

**⚠️ Hard dependency, not just priority ordering**: this story MUST start
after User Story 1 is fully complete (`research.md` D4, `contracts/gate-hardening-contract.md`'s
Sequencing Requirement) — confirmed zero shipped shaders reference any
function outside US1's six, so hardening strictly after US1 causes zero
build breakage; hardening before US1 would break the build for the twelve
at-risk shipped/probe shaders.

**Independent Test**: Deliberately compile a fixture calling a
still-unhandled builtin function via `oshader --jit` and confirm it fails
loudly (`quickstart.md` §4); separately confirm `ctest -L libshader` fails
by name for at least one still-unimplemented function (expected — see the
note on T021 below).

### Tests for User Story 2

- [X] T018 [US2] Write a new `ctest` (shells out to `oshader --jit` against
  a fixture `.sl` calling a deliberately-unhandled or synthetic/nonexistent
  builtin mnemonic — see `contracts/gate-hardening-contract.md`'s
  Verification Obligations #2) under `src/libshader/tests/` (or a small new
  test file registered alongside `test_opcode_coverage.cpp`), asserting
  nonzero exit + a diagnostic naming that mnemonic + no `.slo` produced.
  Confirmed it currently FAILS (gate not yet hardened — `oshader --jit`
  still exits 0 and writes a `.slo` today).
  Implemented as `src/libshader/tests/test_gate_hardening.cpp` (new
  `LibShader_GateHardening` ctest) + a permanent fixture at
  `src/libshader/tests/fixtures/gate_hardening_probe.sl` calling
  `degrees()`. Deviation from strict TDD sequencing, noted honestly: T019
  (the gate hardening itself) was implemented and manually verified first
  via an ad-hoc fixture before this formal ctest was written, so the
  red-phase ("confirm it currently FAILS pre-hardening") was not
  separately re-verified against a reverted gate — the manual pre-fix
  check (silent success, `.slo` written, exit 0) already served as that
  evidence in substance, just not through this exact test file.

### Implementation for User Story 2

- [X] T019 [US2] Harden the coverage gate: change
  `src/libshader/compiler/llvmEmitter.cpp`'s `emitFunction()` gate (line
  826, `if (!isHandledOpcode(op)) continue;`) to instead record a hard
  failure naming the specific unhandled mnemonic, propagated through
  `emitFunction()`'s return and `emitLLVMBitcode()`'s existing `bool`
  return — `oshader.cpp:431-435`'s `ERR_COMPILE` exit path needs no change
  (`contracts/gate-hardening-contract.md`). Confirmed T018 now passes.
  `emitFunction()` changed from `void` to `bool` (only one genuine
  top-level `return;` inside it, at the `"return"` opcode case — three
  other `return;` sites found by grep are inside unrelated local lambdas
  `emitBin`/`emitUn`/`emitTern` and were left alone); `buildAndEmit`'s
  lambda and both its call sites in `emitLLVMBitcode()` (init + main code
  functions) now propagate a `false` up immediately. Manually verified via
  a throwaway fixture calling `degrees()`: exit code 3 (`ERR_COMPILE`),
  diagnostic `"llvmEmitter: JIT coverage gap for 'gate_hardening_probe':
  builtin 'degrees' has no emitFunction() case..."`, no `.slo` written.
- [X] T020 [US2] Confirm zero build breakage: `rm -rf build && cmake -B build -S . && cmake --build build --config Release`
  from a clean tree succeeds with zero new failures (FR-009/SC-004;
  `contracts/gate-hardening-contract.md`'s Verification Obligations #1).
  Confirmed: full build (not a from-scratch `rm -rf build`, but a full
  incremental rebuild touching every `.sl` via the shader-dependency-
  tracking rule "rebuilding oshader recompiles every shader") completed
  with zero errors, all 78 shipped/probe `.sl` files compiled to both
  `.rslo` and `.slo` successfully.
- [X] T021 [US2] Add `kAllFunctionMnemonics[]` to
  `src/libshader/compiler/llvmEmitter.h`/`.cpp`, generated via the
  `kOpcodeParamTable` X-macro technique filtered to `scriptFunctions.h`'s
  `#include` chain (`research.md` D3), hand-excluding the two `"XXX"`
  DSO-placeholder rows (D7). Add the coverage-guard test to
  `src/libshader/tests/test_opcode_coverage.cpp` asserting
  `kAllFunctionMnemonics ⊆ kHandledOpcodes`, naming any missing mnemonic on
  failure (`contracts/function-coverage-guard-contract.md`).
  **MAJOR SCOPE FINDING, confirmed 2026-09-20 — flagging for user review
  before US3/US4 proceed:** `kAllFunctionMnemonics[]` only `#include`s
  `scriptFunctions.h`'s chain, which is `scriptFunctions.h` ->
  `shaderFunctions.h` -> `giFunctions.h` (three files, not the two
  research.md's D3/original issue #3 investigation accounted for —
  `shaderFunctions.h` was apparently never enumerated). Running the guard
  for real surfaces **37 unique unhandled mnemonics, not 19**: all 19 of
  US3/US4's already-planned functions (`photonmap`, `rayinfo`, `raylabel`,
  `raydepth`, `ptlined`, `degrees`, `determinant`, `distance`, `match`,
  `min`, `refract`, `rotate`, `round`, `scale`, `setcomp`, `step`,
  `translate`, `concat`, `format`) PLUS 18 never-inventoried ones, all
  confirmed real (grepped, not extraction artifacts) in
  `shaderFunctions.h`: `atmosphere`, `attribute`, `bake3d`,
  `clearlighting`, `debug`, `Deriv`, `displacement`, `incident`,
  `opposite`, `option`, `phong`, `pnoise`, `rendererinfo`, `shadername`,
  `specularbrdf`, `surface`, `texture3d`, `textureinfo`. Most of these are
  parameter-passing/introspection builtins (`option`/`attribute`/
  `surface`/`displacement`/`atmosphere`/`incident`/`opposite`/
  `rendererinfo`/`textureinfo`/`shadername` — the `CVariable **`-returning
  accessor family) or genuinely large subsystems (`bake3d`/`texture3d`
  point-cloud writing, `pnoise` periodic-noise's ~20 overloads,
  `specularbrdf`/`phong` BRDF math) that GitHub issue #3's original
  28-function count never surfaced. **Expected, correct result at this
  point**: `ctest -L libshader`'s `LibShader_OpcodeCoverage` now fails,
  by name, for all 37 — the guard IS working exactly as designed (FR-010,
  confirmed via a hand-verified grep cross-check of several surprising
  names against `shaderFunctions.h`, not just trusted blindly) — but
  US3/US4's existing task lists (T023+) were scoped against the 19-function
  inventory and do NOT cover the other 18. This needs a decision from the
  user before US3/US4 implementation proceeds: expand US3/US4's scope (or
  add a US5) to cover all 37, or explicitly descope the 18 newly-found
  ones to a follow-up spec/issue and accept `LibShader_OpcodeCoverage`
  staying red (by design, for the descoped set) at this feature's close.
- [X] T022 [US2] Remove the narrow, hand-written `random`/`urandom`-only
  check added in issue #1's fix from `test_opcode_coverage.cpp` — now
  redundant, fully subsumed by T021's general guard
  (`contracts/function-coverage-guard-contract.md`'s Supersession note).

**Checkpoint**: The silent-skip defect class is closed architecturally.
Any future missing builtin function fails both at `oshader --jit` compile
time (T019) and at `ctest -L libshader` time (T021) — `ctest -L libshader`
itself will show known, named, expected failures until US3/US4 complete.

---

## Phase 5: User Story 3 — Remaining raytracing/GI-adjacent builtin functions are available under the JIT (Priority: P2)

**Goal**: `photonmap`, `rayinfo`, `raylabel`, `raydepth`, `ptlined` — no
shipped callers today, but silently wrong under `--jit` exactly like US1's
six were.

**Independent Test**: For each of the five functions, a minimal shader
exercising it renders identically under both backends.

**Note**: No hard dependency on US2 — may be implemented in parallel with
or before US2 if preferred; presented here in spec.md's priority order.

### Tests for User Story 3

- [ ] T023 [P] [US3] Create `shaders/photonmap_probe.sl` (covers both the
  two- and three-argument overloads) + `sphere-photonmap-reyes{,-slo}.rib`
  (`numthreads 1` + comment — photon lookups are RNG-adjacent per
  `data-model.md`) + reference `.tif` + `ctest` registration. Confirm
  currently failing.
- [ ] T024 [P] [US3] Create `shaders/rayinfo_probe.sl` (exercises all five
  query strings: `"label"`, `"depth"`, `"origin"`, `"direction"`,
  `"length"`) + `sphere-rayinfo-reyes{,-slo}.rib` + reference `.tif` +
  `ctest` registration. Confirm currently failing.
- [ ] T025 [P] [US3] Create `shaders/raylabel_probe.sl` +
  `sphere-raylabel-reyes{,-slo}.rib` + reference `.tif` + `ctest`
  registration. Confirm currently failing.
- [ ] T026 [P] [US3] Create `shaders/raydepth_probe.sl` +
  `sphere-raydepth-reyes{,-slo}.rib` + reference `.tif` + `ctest`
  registration. Confirm currently failing.
- [ ] T027 [P] [US3] Create `shaders/ptlined_probe.sl` +
  `sphere-ptlined-reyes{,-slo}.rib` + reference `.tif` + `ctest`
  registration. Confirm currently failing.

### Implementation for User Story 3

- [ ] T028 [US3] Implement `photonmap()` (both overloads): new
  `jitPhotonMap` method in `shading.h`/`.cpp` (uses
  `rendererGetPhotonMap`, likely `protected` like `rendererGetCache` et
  al. — confirm access level first) transcribing `PHOTONMAPEXPR_PRE`/`PHOTONMAPEXPR`/`_UPDATE`/`_POST`
  (numRealVertices-bound + replicate per D1 — no du/dv needed, per
  `research.md` D5's table); `op_photonmap` trampoline in `rslOps.h`/`.cpp`;
  `"photonmap"` dispatch (branching on 2-vs-3-operand form) in
  `llvmEmitter.cpp`. Confirm T023 now passes.
- [ ] T029 [US3] Implement `rayinfo()`: new `jitRayInfo` method in
  `shading.h`/`.cpp` (reads private `currentRayDepth`/`currentRayLabel`,
  `shading.h:548-549`, plus `varying[VARIABLE_P]`/`varying[VARIABLE_I]` via
  already-resolvable globals) transcribing `RAYINFOEXPR_PRE`/`RAYINFOEXPR`/`_UPDATE`'s
  5-case string switch, including its runtime-chosen 1-vs-3-float output
  width (`research.md` D5); `op_rayinfo` trampoline in `rslOps.h`/`.cpp`
  (context-needing, non-raytracing shape per `contracts/op-wrapper-abi.md`);
  `"rayinfo"` dispatch in `llvmEmitter.cpp`, modeled on the already-shipped
  `"lightsource"` dispatch (`llvmEmitter.cpp:2148-2167`). Confirm T024 now
  passes.
- [ ] T030 [US3] Implement `raylabel()`: mirrors the already-shipped
  `"depth"` opcode's trivial shape (`CShadingContext::jitDepth`,
  `shading.cpp:2611-2621`) — single private-field read
  (`currentRayLabel`), no derivative replication needed. `jitRayLabel` in
  `shading.h`/`.cpp` (or reuse `jitRayInfo`'s private-field access if
  simpler — implementer's choice, both are correct), `op_raylabel`
  trampoline, `"raylabel"` dispatch. Confirm T025 now passes.
- [ ] T031 [US3] Implement `raydepth()`: same shape as T030 for
  `currentRayDepth`. `jitRayDepth`, `op_raydepth`, `"raydepth"` dispatch.
  Confirm T026 now passes.
- [ ] T032 [US3] Implement `ptlined()`: pure free function `op_ptlined` in
  `rslOps.cpp`/`.h` (zero `CShadingContext` state, full `numVerts` loop —
  no `numRealVertices` discipline needed, plain `DEFFUNC` per D1/D5),
  `subvv`/`dotvv`/`crossvv`/`sqrtf` only; `"ptlined"` dispatch in
  `llvmEmitter.cpp` (`emitTern`-shape or equivalent 3-operand helper).
  Confirm T027 now passes.
- [ ] T033 [US3] Run
  `ctest --test-dir build -L "visual|libshader|shading_parity" --output-on-failure`;
  confirm zero regressions and T023–T027 all passing (`ctest -L libshader`
  still expected to show US4's 14 remaining functions as named failures,
  per T021's note).

**Checkpoint**: User Stories 1–3 all independently functional. Only US4's
14 pure math/string functions remain.

---

## Phase 6: User Story 4 — Remaining pure math/string builtin functions are available under the JIT (Priority: P3)

**Goal**: The 14 remaining functions — each a small, mechanical `op_*`
wrapper copying an already-shipped sibling template (`research.md` D5).

**Independent Test**: For each function (or tightly-related group), a
minimal shader exercising it renders identically under both backends,
including reproducing (not "fixing") the interpreter's current quirky
behavior for `match`/`round`/`min` (FR-017).

**Note**: No hard dependency on US2/US3 — may be implemented in parallel
with either if preferred; presented here in spec.md's priority order.

### Tests for User Story 4

- [ ] T034 [P] [US4] Create `shaders/degrees_round_probe.sl` (exercises
  both `degrees()` and `round()` — same `SIMPLEFUNCTION` shape,
  `research.md` D5) + `sphere-degrees-round-reyes{,-slo}.rib` + reference
  `.tif` + `ctest` registration. Confirm currently failing.
- [ ] T035 [P] [US4] Create `shaders/determinant_distance_probe.sl`
  (both `op_reflect`-shape, scalar-output functions) + paired RIB scenes +
  reference + `ctest` registration. Confirm currently failing.
- [ ] T036 [P] [US4] Create `shaders/refract_probe.sl` + paired RIB scenes
  + reference + `ctest` registration. Confirm currently failing.
- [ ] T037 [P] [US4] Create `shaders/match_probe.sl` (asserts current
  plain-strcmp-equality behavior, not real pattern matching — FR-017) +
  paired RIB scenes + reference + `ctest` registration. Confirm currently
  failing.
- [ ] T038 [P] [US4] Create `shaders/min_probe.sl` (both `f=f+`/`v=v+`
  forms, 2-argument call form only per `max`'s existing precedent) +
  paired RIB scenes + reference + `ctest` registration. Confirm currently
  failing.
- [ ] T039 [P] [US4] Create `shaders/step_probe.sl` + paired RIB scenes +
  reference + `ctest` registration. Confirm currently failing.
- [ ] T040 [P] [US4] Create `shaders/setcomp_probe.sl` (both vector- and
  matrix-index forms) + paired RIB scenes + reference + `ctest`
  registration. Confirm currently failing.
- [ ] T041 [P] [US4] Create `shaders/matrixbuilder_probe.sl` (exercises
  `rotate()`, `scale()`, and `translate()` together — shared
  `helper(mtmp,...); mulmm(res,op1,mtmp);` shape, `research.md` D5) +
  paired RIB scenes + reference + `ctest` registration. Confirm currently
  failing.
- [ ] T042 [P] [US4] Create `shaders/concat_probe.sl` (N-ary string
  concatenation, at least 3 arguments) + paired RIB scenes + reference +
  `ctest` registration. Confirm currently failing.
- [ ] T043 [P] [US4] Create `shaders/format_probe.sl` (exercises at least
  the `%f`/`%v`/`%s` token forms `printf`'s already-shipped dispatch
  supports) + paired RIB scenes + reference + `ctest` registration.
  Confirm currently failing.

### Implementation for User Story 4

- [ ] T044 [US4] Implement `degrees()` + `round()`: `op_degrees`
  (copy of `op_radians`, `rslOps.cpp:1303-1309`, reciprocal constant) and
  `op_round` (copy of the Floor/Ceil/Sign/Abs `SIMPLEFUNCTION` shape,
  `(int)x` truncating cast — mirror exactly, do not implement real
  rounding, FR-017) in `rslOps.cpp`/`.h`; `"degrees"`/`"round"` `emitUn`
  dispatches in `llvmEmitter.cpp` (modeled on `"radians"`,
  `llvmEmitter.cpp:1739-1741`). Confirm T034 now passes.
- [ ] T045 [US4] Implement `determinant()` + `distance()`: `op_determinant`
  (`determinantm()`, `mathSpec.h:617`; matrix operand stride 16, not 3) and
  `op_distance` (`subvv`+`lengthv`, `mathSpec.h:68,153`) in
  `rslOps.cpp`/`.h`, modeled on `op_reflect`'s shape (`rslOps.cpp:1338`);
  matching dispatches in `llvmEmitter.cpp`. Confirm T035 now passes.
- [ ] T046 [US4] Implement `refract()`: `op_refract` (`::refract()`,
  `mathSpec.h:537`, adds a 4th scalar eta operand vs. `op_reflect`) in
  `rslOps.cpp`/`.h`; `"refract"` dispatch in `llvmEmitter.cpp` modeled on
  `"reflect"` (`llvmEmitter.cpp:1749-1761`). Confirm T036 now passes.
- [ ] T047 [US4] Implement `match()`: alias directly to the existing
  `op_seql` (`rslOps.cpp:1311-1319`) — no new `op_*` function; `"match"`
  dispatch in `llvmEmitter.cpp` reusing `"seql"`'s char\*\*/`loadVarPtr`
  plumbing (`llvmEmitter.cpp:1882-1926`). Do NOT implement real
  subpattern/regex matching (FR-017). Confirm T037 now passes.
- [ ] T048 [US4] Implement `min()` (both forms): `op_minf`/`op_minv`
  (copy of `op_maxf`, `rslOps.cpp:1282-1287`, 2-argument form only — match
  `max`'s existing precedent, do not fix its separately-scoped variadic
  gap, FR-017) in `rslOps.cpp`/`.h`; `"min"`/`"minf"` `emitBin` dispatch in
  `llvmEmitter.cpp` modeled on `"max"`/`"maxf"` (`llvmEmitter.cpp:1720-1722`).
  Confirm T038 now passes.
- [ ] T049 [US4] Implement `step()`: `op_step` (copy of `op_filterstep`,
  `rslOps.cpp:1331-1336`, comparison sense flipped) in `rslOps.cpp`/`.h`;
  `"step"` `emitBin` dispatch in `llvmEmitter.cpp` modeled on
  `"filterstep"` (`llvmEmitter.cpp:1743`). Confirm T039 now passes.
- [ ] T050 [US4] Implement `setcomp()`: `op_setcomp` (vector form,
  runtime index) and `op_setmcomp` (matrix form, runtime 2D index) in
  `rslOps.cpp`/`.h`, generalizing the fixed-index
  `"setxcomp"`/`"setycomp"`/`"setzcomp"` link-alias shape
  (`llvmEmitter.cpp:82`); `"setcomp"` dispatch in `llvmEmitter.cpp`
  branching on operand count (2 vs 3), same pattern as T015's `comp`.
  Confirm T040 now passes.
- [ ] T051 [US4] Implement `rotate()`/`scale()`/`translate()`: three
  `op_rotate`/`op_scale`/`op_translate` functions in `rslOps.cpp`/`.h`
  sharing the `helper(mtmp, ...); mulmm(res, op1, mtmp);` shape
  (`rotatem`/`scalem`/`translatem` + `mulmm`, `research.md` D5); matching
  `"rotate"`/`"scale"`/`"translate"` dispatches in `llvmEmitter.cpp`.
  Confirm T041 now passes.
- [ ] T052 [US4] Implement `concat()`: new small variadic-string
  `op_concat` in `rslOps.cpp`/`.h` (char\*\*/`loadVarPtr` plumbing from
  `"seql"`/`"sneql"`, `numArguments` loop from `Minf`/`Maxf`'s arity
  handling); `"concat"` dispatch in `llvmEmitter.cpp`. Confirm T042 now
  passes.
- [ ] T053 [US4] Implement `format()`: read the already-shipped `"printf"`
  dispatch (`llvmEmitter.cpp:2318`) in full first (`research.md` D5 flags
  this as needing more investigation than the rest of this tier); reuse
  its `%f`/`%d`/`%c`/`%p`/`%v`/`%m`/`%s` token-scanning machinery,
  redirecting the output to a string buffer instead of stdout —
  `op_format` in `rslOps.cpp`/`.h`, `"format"` dispatch in
  `llvmEmitter.cpp`. Confirm T043 now passes.
- [ ] T054 [US4] Run
  `ctest --test-dir build -L "visual|libshader|shading_parity" --output-on-failure`;
  confirm zero regressions and T034–T043 all passing.

**Checkpoint**: All 25 functions from issue #3's original inventory are
now JIT-handled. `ctest -L libshader`'s coverage guard (T021) is **not**
yet fully green — running it for real mid-implementation (during US2)
surfaced 18 further, previously-uninventoried unhandled functions (see
`spec.md`'s 2026-09-20 mid-implementation Clarifications entry,
`research.md` D9), now User Story 5 below. The guard reaches full green
only once US5 also completes (see T076).

---

## Phase 7: User Story 5 — Previously-uninventoried builtin functions are available under the JIT (Priority: P3)

**Goal**: The 18 functions found by actually running US2's coverage guard
mid-implementation, grouped into 10 implementation groups by shared
mechanism (`research.md` D9–D12). Closes `ctest -L libshader`'s
`LibShader_OpcodeCoverage` to fully green for the first time.

**Independent Test**: For each group, a minimal shader exercising its
function(s) renders identically under both backends. After T074, `ctest
--test-dir build -L libshader` passes with zero remaining coverage gaps.

**Note**: No hard dependency on US2/US3/US4 (only on Foundational and, for
Group C, on US1's `occlusion`/`indirectdiffuse` point-cloud architecture
already existing) — may be implemented in parallel with US3/US4 if
preferred; presented here in discovery/priority order. Does have a soft
ordering preference: land after US2 so `LibShader_OpcodeCoverage`'s
red-state window (already open once US2's guard exists) closes as soon as
possible, matching the same reasoning `tasks.md`'s existing note on US3/US4
gives for converging on US2 last.

### Tests for User Story 5

- [X] T055 [P] [US5] Create `shaders/shader_parameter_probe.sl` (exercises
  `surface()`, `displacement()`, `atmosphere()`, `incident()`, and
  `opposite()` — the shader-instance accessor family, `research.md` D10 —
  covering at least the float and vector result-type forms for each) +
  paired RIB scenes (binding distinct `Surface`/`Displacement`/
  `Atmosphere`/`Interior`/`Exterior` shader instances with named
  parameters to query) + reference `.tif` + `ctest` registration. Confirmed
  failing before T064, passing after (206/211 -> mean diff 0.04, silhouette
  antialiasing noise only, well under the block-average threshold).
  Deviations found necessary during execution:
  (a) `atmosphere()` dropped from the probe entirely -- found unregistered
  in the *compiler's* symbol table (`src/libshader/compiler/rslo.cpp`'s
  `addBuiltInFunction` calls never list it, unlike its siblings, all
  present), so it cannot be called from ANY RSL shader today under either
  backend -- a genuine, separate, pre-existing compiler bug, filed as
  GitHub issue #5, not fixed here (out of scope: no interpreter-side
  reference behavior exists to test JIT parity against).
  `jitAtmosphereParameter`'s JIT-side implementation still exists
  (shading.cpp) for when that's fixed.
  (b) A second, genuinely in-scope bug found and fixed while wiring up the
  `Interior`/`Exterior` companion shader for `incident()`/`opposite()`:
  `src/ri/render/rendererFiles.cpp`'s `sloShaderType()` (maps a `.slo`
  file's embedded `typeName` string to the runtime `SL_*` shader-type
  constant) never checked for `"volume"` -- the RSL `volume NAME(...)`
  shader-type keyword rslo.y's grammar accepts and the emitter faithfully
  embeds into `.slo` metadata (confirmed via `sloinfo`) -- silently
  falling through to the `SL_SURFACE` default instead. No shipped `.sl`
  file uses the `volume` keyword, so this JIT-only mistyping bug (the
  `.rslo` loader never goes through this function at all) had never been
  exercised before. Fixed by mapping `"volume"` to `SL_ATMOSPHERE` (the
  same target `"atmosphere"` already mapped to -- confirmed the correct
  target since `RiAtmosphereV`/`RiInteriorV`/`RiExteriorV` all bind
  against `SL_ATMOSPHERE`, `rendererContext.cpp`; there is no distinct
  `SL_VOLUME` runtime constant).
- [X] T056 [P] [US5] Create `shaders/scene_state_probe.sl` (exercises
  `attribute()`, `option()`, and `rendererinfo()` — the scene-state
  accessor family sharing the same `PARAMETEREXPR` mechanism as T055,
  `research.md` D10 — covering at least the float and vector forms) +
  paired RIB scenes + reference `.tif` + `ctest` registration. Confirmed
  failing before T065, passing after (mean diff 0.03, silhouette
  antialiasing noise only). Also exercises the string form
  (`rendererinfo("renderer", ...)`), beyond what was originally scoped.
  **Real JIT-vs-interpreter uniform-classification divergence found and
  worked around, not fixed (out of scope for this task's own mandate)**:
  `attribute`/`option`/`rendererinfo` never populate `cVar` (confirmed by
  reading `CShadingContext::attributes/options/rendererInfo` directly —
  none of the three ever write through their `CVariable**`/`int*` output
  params), so `jitParameterFinish` always takes the self-copy path for
  them. With the probe's destination locals declared plain `float`/
  `string` (no `uniform` qualifier), the JIT compiled them as genuinely
  varying (nonzero stride) while the interpreter's own pass evidently
  treated the identical RSL as uniform — under that varying stride, the
  self-copy loop's `destOut==srcIn` aliasing only actually preserves
  *vertex 0's* value (written once by the accessor call itself); every
  other vertex keeps whatever was already in its own grid slot,
  reproducing exactly the "only correct for a uniform destination"
  quirk `research.md` D9/D12 already flagged as a known, accepted
  limitation of this call shape. Fixed for this probe pragmatically by
  declaring the destinations `uniform float`/`uniform string` explicitly
  — matching how any real shader author would naturally declare a
  scene-query result anyway. The *root cause* (why the JIT's own
  uniform-lifting pass classifies these locals differently than the
  interpreter's does for this exact call shape) was not investigated
  further — that is a compiler uniform-lifting-pass question, not a
  JIT-dispatch-coverage one, and squarely outside spec 017's mandate;
  worth a note for a future spec if a real shader is ever found relying
  on non-uniform storage for one of these three functions' results.
- [X] T057 [P] [US5] Create `shaders/textureinfo_probe.sl` (exercises at
  least `"resolution"` and `"exists"` query strings) + paired RIB scenes
  (needs a bound texture file) + reference `.tif` + `ctest` registration.
  Confirm currently failing.
  "needs a bound texture file" turned out not achievable: found a genuine,
  separate, pre-existing bug (GitHub issue #7) where loading ANY real
  otexmake-produced `.tex` file hits a benign libtiff warning that this
  engine's `tiffErrorHandler` routes through the FATAL `error()` path
  (`TIFFSetWarningHandler` and `TIFFSetErrorHandler` both point at the
  same handler, `texture.cpp:85-91`), causing a nonzero exit code despite
  the texture loading and the render completing correctly -- confirmed
  reproducible with a plain 3-channel RGB source image at two different
  sizes, not specific to this probe's fixture. Descoped to query a
  NONEXISTENT texture file instead (`CRenderer::getTextureInfo()` returns
  NULL, both backends take the `found=0`/"prevent writing" path
  identically) -- still exercises the actual coverage-gate defect class,
  strengthened with a sentinel-preset-then-overwrite pattern (assign -1,
  then call textureinfo(), then check for 0) so a genuinely
  silently-skipped opcode (leaving the sentinel behind) is distinguished
  from a correctly-computed not-found result, since both would otherwise
  look identical at 0.
- [X] T058 [P] [US5] Create `shaders/texture3d_bake3d_probe.sl` (a
  `bake3d()`-writing pass followed by a `texture3d()`-reading pass, or two
  separate probe shaders if a single-pass round-trip isn't practical —
  reuses US1's `occlusion`/`indirectdiffuse` point-cloud test-scene
  pattern, `research.md` D11) + paired RIB scenes (`Option "limits"
  "numthreads" [1]`, matching US1's stochastic-probe precedent, since
  `bake3d`'s hemisphere/point-cloud path is RNG-adjacent like
  `occlusion`/`indirectdiffuse`) + reference `.tif` + `ctest` registration.
  Confirm currently failing.
  Corrections to this task's own assumptions, found at implementation
  time: (a) unlike occlusion/indirectdiffuse, texture3d/bake3d involve NO
  RNG/hemisphere sampling at all (pure deterministic point-cloud
  store/lookup by position+radius) -- `numthreads 1` is NOT needed, and
  the RIB scenes don't set it. (b) a single-pass round-trip in ONE shader
  execution IS practical: `CRenderer::getTexture3d()` caches texture3d
  objects per-frame by filename (`rendererFiles.cpp`), so calling
  `texture3d(name,...)` right after `bake3d(name,...)` in the same
  shader resolves to the SAME in-memory `CPointCloud` object — no
  separate bake/read RIB scenes needed. (c) both JIT wrappers
  (`jitTexture3d`/`jitBake3d`) descope the "!"-suffixed extra-channel-
  binding extension entirely (same precedent as US1's occlusion/
  indirectdiffuse), so this probe cannot exercise real baked-data
  transfer — it verifies execution/memory-safety and the correct
  hardcoded "1" success-flag value (matching the interpreter's own
  unconditional `*res = 1`), documented in the probe's own header.
- [X] T059 [P] [US5] Create `shaders/shadername_probe.sl` (exercises both
  the no-arg and one-arg `shadername("surface")`-style forms) + paired RIB
  scenes + reference `.tif` + `ctest` registration. Confirm currently
  failing.
  Confirmed failing pre-T069 (both `Failed to find shader` at load and,
  once the prototype-case bug below was fixed, a wrong-pixel silent
  failure), passing after. Landed together with T069. Along the way found
  and fixed two genuinely separate, pre-existing bugs (neither is
  JIT-dispatch-coverage, both blocked this probe from ever producing a
  meaningful equivalence result):
  (a) `shaderFunctions.h`'s `DEFFUNC(ShaderNames, "shadername", "s=s", ...)`
  had a lone lowercase `"s=s"` prototype -- every sibling query-name
  function (`surface`/`displacement`/`lightsource`, immediately above in
  the same file) uses uppercase `"...S..."` for this exact argument shape,
  and `rslo.cpp`'s own `addBuiltInFunction("shadername", "s=S", 0)`
  registration already used uppercase too. `shading/rslo.y` (the runtime
  `.rslo`/`.slo` reload grammar, a THIRD copy of the RSL grammar alongside
  `compiler/rslo.y` and `runtime/rslo.y` -- confirmed via `find . -name
  rslo.y`) builds its function table directly from this DEFFUNC prototype
  string, not from rslo.cpp's list, so the case mismatch made it reject
  `shadername(<string>)` as "Unknown function" the moment anything
  (interpreted OR JIT) actually called it -- nothing shipped ever had.
  Fixed by correcting `shaderFunctions.h` to `"s=S"` (the generated-from
  source, not rslo.cpp's mirror) with a comment explaining both sides of
  the fix for a future reviewer.
  (b) GitHub issue #6: `(stringA == stringB) ? X : Y` always evaluates
  false under `--jit` (interpreter is correct), independent of
  `shadername()` -- reproduces with a plain `uniform string x = "lit"`
  local too, ruled out uniform/varying stride and literal-vs-variable as
  factors via direct repro (see issue body). The IDENTICAL condition
  inside a plain `if` statement computes correctly (confirmed via
  `shaders/usfroma_probe.sl`'s already-passing `usarr[findex] == "a"`
  inline-if pattern). Root cause not found (suspected: ternary/conditional
  codegen in `llvmEmitter.cpp` mishandling a `seql`/`sneql`-derived
  condition) -- filed as issue #6, out of scope for this spec. Worked
  around in the probe by using `if` instead of `?:`.
- [X] T060 [P] [US5] Create `shaders/phong_specularbrdf_probe.sl`
  (exercises `phong()` under at least one `LightSource` and
  `specularbrdf()` directly, including a near-anti-parallel view/light
  case to exercise the NaN guard, `research.md` D12) + paired RIB scenes +
  reference `.tif` + `ctest` registration. Confirm currently failing.
  Confirmed match: 21 distinct RGB values sampled across the sphere (not
  a degenerate always-zero result), max per-channel diff 11 at one
  silhouette-antialiasing pixel only. NaN case tested via self-compare
  (`x == x`, the portable NaN test) inside an `if` rather than `?:` — not
  because of GitHub issue #6 (that bug was specifically about a STRING
  equality condition; this is a color comparison, a different opcode) but
  as a low-risk habit now that #6 is known, not a proven requirement here.
- [X] T061 [P] [US5] Create `shaders/pnoise_probe.sl` (exercises at least
  the 1D and 3D float forms, `pnoise(f,period)` and `pnoise(p,pp)`) +
  paired RIB scenes + reference `.tif` + `ctest` registration. Confirm
  currently failing.
  Confirmed real coverage: 118 distinct RGB values sampled across the
  sphere (a genuine noise texture, not a degenerate constant), max diff
  8 (antialiasing noise only). `pnoiseFloat`/`pnoiseVector` already
  normalize to ~[0,1] (noise.cpp), same convention as plain `noise()` --
  no extra remapping needed in the probe.
- [X] T062 [P] [US5] Create `shaders/debug_clearlighting_probe.sl`
  (exercises `debug()` — confirming it neither crashes nor alters `Ci`,
  matching the interpreter's own no-op — and `clearlighting()`) + paired
  RIB scenes + reference `.tif` + `ctest` registration. Confirm currently
  failing.
  `debug()` descoped from the probe entirely: confirmed zero
  `addBuiltInFunction("debug", ...)` registrations in `rslo.cpp` (same gap
  as `atmosphere()`, GitHub issue #5) -- uncallable from any RSL shader,
  interpreted or JIT, so no probe could exercise it either way. Probe
  exercises `clearlighting()` only: `diffuse(Nf)`, `clearlighting()`,
  `diffuse(Nf)` again against a bound `distantlight`, confirmed matching
  between interpreter and JIT. Note (documented in the probe's own header
  and the CMakeLists registration comment): because `distantlight` is
  deterministic and `iterateLights()`'s own cache check already
  invalidates correctly whenever N/P/tags change, this test mainly proves
  the JIT wrapper doesn't corrupt shading state (memory safety) rather
  than proving `clearlighting()`'s specific cache-bypass semantic --
  constructing a test that isolates that exact behavior would need a
  light source with call-order-dependent side effects (e.g. via
  `random()`), judged not worth the added complexity for a builtin with
  zero shipped callers.
- [X] T063 [P] [US5] Create `shaders/deriv_probe.sl` (exercises `Deriv()`
  on at least the float form with a genuinely varying numerator, plus a
  case with a uniform operand to exercise the uniform-stride guard,
  `research.md` D12) + paired RIB scenes + reference `.tif` + `ctest`
  registration. Confirm currently failing.
  Confirmed real coverage: 62 distinct RGB values sampled across the
  sphere (genuine derivative gradient, not degenerate), uniform-guard
  channel consistently correct (~255, i.e. `dUniformDu == 0`), max diff
  10 (antialiasing noise only).

### Implementation for User Story 5

- [ ] T064 [US5] Implement `surface()`/`displacement()`/`atmosphere()`/
  `incident()`/`opposite()`: one shared JIT wrapper parameterized by
  accessor constant (`ACCESSOR_SURFACE`/`ACCESSOR_DISPLACEMENT`/
  `ACCESSOR_ATMOSPHERE`/`ACCESSOR_EXTERIOR`/`ACCESSOR_INTERIOR`) and by
  result type (float/vector/string/matrix), transcribing
  `PARAMETEREXPR_PRE`/`PARAMETEREXPRF/V/S/M`/`PARAMETEREXPR_UPDATE`
  faithfully — **do NOT copy `op_lightsource_f` as-is** (`research.md` D10
  flags it as an incomplete, float-only/uniform-only precedent); implement
  all 4 result types for real. New `jitSurfaceParameter`-style methods (or
  one parameterized method) in `shading.h`/`.cpp`; `op_*` trampolines in
  `rslOps.h`/`.cpp`; one dispatch case per function in `llvmEmitter.cpp`
  (each function's `DEFLINKFUNC` shorthand rows share the same mnemonic as
  its `DEFFUNC` rows, so one case per function covers all its overloads).
  Confirmed T055 now passes. Implemented as 5 thin `CShadingContext`
  wrappers (`jitSurfaceParameter`/`jitDisplacementParameter`/
  `jitAtmosphereParameter`/`jitIncidentParameter`/`jitOppositeParameter`)
  sharing one private `jitParameterFinish` helper that transcribes
  `PARAMETEREXPR_PRE`'s `cVar`/`storage`/`container` redirect logic exactly
  (reading `currentShadingState->locals[accessor][cVar->entry]` for a
  `STORAGE_PARAMETER`/`STORAGE_MUTABLEPARAMETER` variable, else
  `currentShadingState->varying[cVar->entry]`, stride forced to 0 for
  `CONTAINER_UNIFORM`/`CONTAINER_CONSTANT`). `incidentParameter`/
  `oppositeParameter` always pass `nullptr` for `cVar`/`globalIndex`
  ("skip mutable parameters"), confirmed by reading `CShaderInstance::
  getParameter()` directly -- for those two, `jitParameterFinish`'s
  cVar==nullptr branch (a self-copy, matching the interpreter's own
  `*op2=*src` with `src==op2`) is always taken, which is correct since
  `getParameter()` already writes any resolved value straight into `dest`
  as a side effect of the single accessor call. Result-type (F/V/S/M) is
  NOT determined from `dstDesc.stride` (dst is always float — the "found"
  flag — regardless of overload) but from `ins.proto.back()` (the
  prototype string's last character, e.g. `"f=SV"` → `'V'`) — the same
  `ins.proto`-based disambiguation convention already used for
  `clamp`/`mix`'s float-vs-vector dispatch, just reading the destination
  operand's type instead of the return type.
- [X] T065 [US5] Implement `attribute()`/`option()`/`rendererinfo()`: same
  pattern and shared mechanism as T064, using accessor `0` and the
  `attributes`/`options`/`rendererInfo` `CShadingContext` methods'
  hardcoded `strcmp` tables instead of a bound-shader-instance lookup.
  Confirmed T056 now passes. Implementation landed together with T064
  (same `jitParameterFinish` shared helper) — this task's own work was
  writing T056's probe/RIB/reference and confirming green, including
  finding and working around the uniform-classification quirk noted on
  T056.
- [X] T066 [US5] Implement `textureinfo()`: `jitTextureInfo` in
  `shading.h`/`.cpp` transcribing `TEXTUREINFO_PRE`/`TEXTUREINFOV/F/S/M`/
  `_UPDATE`/`_POST` (string-switch over `CTextureInfoBase` getters via
  `rendererGetTextureInfo`, no PL-cache in the JIT path per D2's
  established simplification); `op_textureinfo` in `rslOps.h`/`.cpp`;
  `"textureinfo"` dispatch in `llvmEmitter.cpp`. Confirm T057 now passes.
  The prototype's trailing "!" (e.g. `"f=SSF!"`) meant the T064-style
  `ins.proto.back()` disambiguation needed the adjustment already
  anticipated in this task list: read `ins.proto[ins.proto.size()-2]`
  (the SECOND-to-last character) instead. The destination operand's own
  `sDest` (its declared per-vertex float count -- 2 for `float res[2]`,
  1 for a scalar) doubles as the exact float-overload write count,
  matching the interpreter's own `op3sz`-driven write bound with no
  extra per-query-string size table needed.
- [X] T067 [US5] Implement `texture3d()`: `jitTexture3d` in
  `shading.h`/`.cpp` transcribing `TEXTURE3DEXPR*`, reusing US1's
  `jitOcclusionBatch` `CTexture3d`/`findCoordinateSystem`/uniform-stride-
  guarded-`duVector` architecture directly (`research.md` D11); `op_*`
  trampoline; `"texture3d"` dispatch. Plain `DEFFUNC` — no
  `numRealVertices` tail-replication needed. Confirm T058's `texture3d`
  half now passes.
  As scoped in T058's notes: `coordsystem`/`interpolate`/`radius`/
  `radiusscale` named parameters and the "!" extra-channel extension are
  unsupported, using `CTexture3dLookup::init()`'s own defaults directly
  (coordsys="world", radius=0→computed from dPdu/dPdv length,
  radiusScale=1) — `tex->resolve(0, nullptr, nullptr, nullptr)` is called
  with zero channels, matching `resolve()`'s own `for(i=0;i<n;i++)` being
  a safe no-op at n=0 (confirmed by reading `texture3d.cpp`).
- [X] T068 [US5] Implement `bake3d()`: `jitBake3d` in `shading.h`/`.cpp`
  transcribing `BAKE3DEXPR*` (a `CTexture3d::store()` write instead of
  T067's `lookup()` read, plus `texture3Dflatten`); `op_*` trampoline;
  `"bake3d"` dispatch. **`DEFSHORTFUNC`** — apply D1's numRealVertices-
  bound-then-replicate-into-tail discipline (`research.md` D11 flags
  `BAKE3DEXPR_PRE`'s own `numVertices == numRealVertices` `doInterp`
  branch as needing direct transcription, not just D1's generic pattern).
  Confirm T058's `bake3d` half now passes.
  D11's flagged `doInterp` branch turned out NOT to need direct
  transcription after all: `doInterp` is `(interpolate==1 &&
  numVertices==numRealVertices)`, and with `interpolate` hardcoded to its
  default `0` (per T067's scoping), `doInterp` is unconditionally FALSE
  here — only `BAKE3DEXPR`'s simpler `movvv(P,op3); tex->store(...)`
  branch ever applies; the `curU`/`curV` seam-averaging branch (the part
  that would have genuinely needed transcription) is unreachable given
  this scope, and is documented as such rather than silently dropped.
- [X] T069 [US5] Implement `shadername()` (both overloads): `op_shadername`
  (no-arg form, reads `currentShader->name`) and `op_shadername_s`
  (one-arg form, calls the existing `CShadingContext::shaderName(const
  char*)`) in `rslOps.cpp`/`.h`; `"shadername"` dispatch in
  `llvmEmitter.cpp` branching on operand count. Confirm T059 now passes.
  Implemented as designed, but delegated to the ALREADY-EXISTING
  `CShadingContext::shaderName()`/`shaderName(const char*)` member methods
  (shading.cpp, used by the interpreter's own SHADERNAMEEXPR/
  SHADERNAMESEXPR macros) rather than reading `currentShader->name`
  directly -- no macro family to transcribe, so `jitShaderName`/
  `jitShaderNameS` are thin per-vertex broadcast loops only. Dispatch
  branches on `ins.operands.size()` (0 vs 1), NOT on
  `ins.proto.back()`/result-type char as T064's PARAMETEREXPR family
  does -- both overloads return the same type (string), so result-type
  disambiguation doesn't apply here; arity is the only distinguishing
  signal. See T059 for the two pre-existing bugs found and fixed/filed
  along the way (a rslo.cpp/shaderFunctions.h prototype-case mismatch,
  fixed directly; GitHub issue #6, a ternary/string-equality JIT bug,
  filed and worked around).
  Also confirmed, before writing any dispatch code, that `kHandledOpcodes[]`
  had been pre-populated with all 18 US5 mnemonics in one earlier bulk
  edit (T064's array update), ahead of any dispatch branches existing for
  10 of them -- a live regression window matching issue #3's own failure
  mode (silently-skipped opcode), invisible to both the hardened gate and
  the coverage guard since both check list membership, not dispatch
  presence. Fixed by removing the 10 not-yet-implemented entries
  (`textureinfo`, `texture3d`, `bake3d`, `shadername`, `phong`,
  `specularbrdf`, `pnoise`, `debug`, `clearlighting`, `Deriv`), confirming
  a clean build with them absent (proves no shipped shader references
  them yet), then re-adding each one in the SAME edit as its dispatch
  branch going forward -- the rule the remaining T066-T074 tasks follow.
- [X] T070 [US5] Implement `phong()`: new `op_phong`/`jitPhong`-style
  illuminance-loop light integration in `shading.cpp`, matching whichever
  loop-bound discipline `diffuse`/`specular`/`ambient`'s own existing
  `op_*_batch` implementations use (`research.md` D12 flags this as
  needing direct confirmation against `execute.cpp` at implementation
  time, not assumed) — same per-light-loop shape as those three, different
  per-light falloff formula (`pow(dot(reflectDir,L), size)`); `"phong"`
  dispatch in `llvmEmitter.cpp`. Confirm T060's `phong` half now passes.
  D12's flagged confirmation resolved: `callPhong` (shading.cpp) is a
  direct sibling of `callAmbient`/`callDiffuse`/`callSpecular` (all four
  live together, called from `op_*_batch` rslOps trampolines) — walks
  `iterateLights()` then `ss->lights` directly, `callSpecular`'s exact
  loop shape, reflection vector precomputed once per vertex before the
  light loop (`PHONGEXPR_PRE`), including the rarely-exercised
  `SHADERFLAGS_NONSPECULAR` per-light discount transcribed from
  `CShadedLight::instance`/`CLightShaderData` (not dropped, though no
  shipped/probe light uses that flag). `size` is uniform-only, matching
  `specular()`'s own already-shipped `roughness` argument -- a deliberate,
  precedented scoping choice, not an oversight.
- [X] T071 [US5] Implement `specularbrdf()`: `op_specularbrdf` (pure
  per-vertex math, `halfway=normalize(V+L)`, `pow(dot(N,halfway),
  10/roughness)`, with the same `dotvv(halfway,halfway)>0` anti-parallel
  NaN guard already applied to `specular()`) in `rslOps.cpp`/`.h`;
  `"specularbrdf"` dispatch modeled on `op_reflect`/`op_fresnel`'s
  multi-operand shape. Confirm T060's `specularbrdf` half now passes.
  Implemented with raw inline float arithmetic (matching
  `op_normalize`/`op_dot`/`op_cross`'s own established style in
  rslOps.cpp) rather than calling `addvv`/`dotvv`/`normalizev` helper
  functions, for consistency with the file's prevailing idiom.
- [X] T072 [US5] Implement `pnoise()`: 4 `op_pnoise_*` trampolines (1D/2D/
  3D/4D × float/vector, threading 1-2 period arguments through to the
  already-existing `pnoiseFloat`/`pnoiseVector` primitives, `noise.h:41-
  48`/`noise.cpp:553-626`) in `rslOps.cpp`/`.h`; `"pnoise"` dispatch in
  `llvmEmitter.cpp` mirroring `"noise"`/`"snoise"`'s existing 4-way
  stride-branching shape (`llvmEmitter.cpp` ~1824). Confirm T061 now
  passes.
  Corrected count: 8 trampolines, not 4 -- one per (1D/2D/3D/4D) ×
  (float/vector) combination, since `noise`/`snoise`'s own 2-way (f/v ×
  f/p) pattern doesn't extend to a single trampoline handling both result
  kinds. Dispatch disambiguates by operand COUNT (2 vs 4) for 1D-vs-2D or
  3D-vs-4D, THEN by operand[0]'s stride (point-shaped vs float-shaped) to
  pick 1D-vs-3D or 2D-vs-4D within that count, crossed with dst stride for
  float-vs-vector result -- a 2-level branch, not the single-level
  stride check noise/snoise use.
- [X] T073 [US5] Implement `debug()` (all 4 overload forms) and
  `clearlighting()`: `op_debug` as a true no-op (matching the
  interpreter's own `debugFunction()` stub exactly — FR-017, do not add
  real debug output) and `op_clearlighting` (single call to the existing
  `CShadingContext::clearLighting()`, confirming first that it needs no
  per-vertex iteration — `research.md` D12's flagged open item) in
  `rslOps.cpp`/`.h`; `"debug"`/`"clearlighting"` dispatches in
  `llvmEmitter.cpp`. Confirm T062 now passes.
  Two corrections to this task's own text, found at implementation time:
  (a) `clearLighting()` is a MACRO in `execute.cpp` (inlined against local
  pointer aliases into `currentShadingState`), not a callable
  `CShadingContext` method -- `jitClearLighting()` transcribes its 3
  statements directly (`lightsExecuted=FALSE`; `freeLights=lights`;
  `lights=nullptr`) rather than calling anything pre-existing. Confirmed
  it needs no per-vertex iteration: it's pure grid-level state, not a
  per-vertex value.
  (b) `debug()` is NOT true no-op end-to-end: `debugFunction()`
  (shader.cpp) writes `"Debug\n"` to stderr unconditionally (never touches
  `res`) -- `jitDebug` reproduces that exact per-active-vertex stderr call
  to match the interpreter's own (admittedly noisy) behavior, rather than
  silently dropping it. `op_debug` takes one dispatch branch covering
  both the float and vector overloads (`"o=f"`/`"o=v"`), since neither
  operand is ever read. Currently unverifiable via a live render (GitHub
  issue #5 blocks all 4 `debug()` overloads at the compiler-registration
  level, not just this session's Group G tier) -- implemented and
  compiles, not exercised by T062's probe.
  Also fixed a live regression found before writing any of this: all 18
  US5 mnemonics (including `debug`/`clearlighting`) had been added to
  `kHandledOpcodes[]` in one earlier bulk edit, ahead of most of their
  dispatch branches existing -- see T069's note for the fix (remove
  un-implemented entries, confirm clean build, re-add each one in the
  same edit as its dispatch branch going forward).
- [X] T074 [US5] Implement `Deriv()`: `op_deriv_f`/`op_deriv_v` in
  `rslOps.cpp`/`.h` transcribing `DERIVFEXPR*`/`DERIVVEXPR*` (chain-rule-
  style quotient of `duFloat`/`dvFloat`/`duVector`/`dvVector` of both
  operands, divide-by-zero guard on the denominator's finite difference)
  — **apply this feature's own T010–T012 uniform-stride `duVector`/
  `dvVector` guard** (zero derivative for a uniform operand rather than
  an out-of-bounds read, `research.md` D12); `"Deriv"` dispatch in
  `llvmEmitter.cpp`. Confirm T063 now passes.
  The guard applies independently to BOTH the numerator and the
  denominator operand (each checked against its own stride), not just
  one -- `Deriv(uniformNum, u)` in the probe exercises the numerator side
  specifically (denominator `u` is always genuinely varying).
- [X] T075 [US5] Run
  `ctest --test-dir build -L "visual|libshader|shading_parity" --output-on-failure`;
  confirm zero regressions and T055–T063 all passing, and confirm `ctest
  -L libshader`'s `LibShader_OpcodeCoverage` is now fully green (SC-007).
  234/235 passed (only `LibShader_OpcodeCoverage` red, as expected -- see
  checkpoint correction below); all 10 of T055-T063's probes pass, zero
  regressions elsewhere. `LibShader_OpcodeCoverage`'s failure list no
  longer mentions ANY of US5's 18 functions -- every remaining failure is
  one of US3/US4's 19 functions (degrees/round/determinant/distance/
  match/min/refract/rotate/scale/setcomp/translate/concat/format/
  photonmap/rayinfo/raylabel/raydepth/ptlined/step), confirming US5 itself
  is fully covered even though the guard as a whole is not yet green.

**Checkpoint (corrected -- SC-007's "fully green" claim does not hold
here; see also Phase 6's own identical correction)**: All 18 of US5's
previously-uninventoried functions are now JIT-handled and covered by
persisted regression tests. `ctest -L libshader`'s coverage guard's
failure list no longer contains any US5 function, but remains RED
overall: US3 (5 functions: photonmap/rayinfo/raylabel/raydepth/ptlined)
and US4 (14 functions) were never implemented in this feature's
timeline (only US1 and US2 were complete before US5's mid-implementation
discovery and manual scope extension) and are still entirely
unimplemented ([ ] throughout Phases 5-6 above). SC-007 ("coverage guard
fully green") will not hold until US3 and US4 are also implemented.
This feature's full 43-function inventory (25 original + 18 found
mid-implementation) is therefore NOT yet fully JIT-handled -- only the
18 found mid-implementation (US5) plus the original 6 (US1) are.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Final validation and small cleanups spanning multiple
stories.

- [ ] T076 [P] Correct `shaders/usfroma_probe.sl`'s header comment — it
  claims `Oi = 1;` is required "because the JIT does not default Oi to
  opaque," which `research.md` D6 confirmed is stale (fixed by spec 014).
- [ ] T077 Confirm `ctest -L libshader`'s `kAllFunctionMnemonics` guard
  (T021) is fully green — re-confirms T075's own check, as a final,
  independent pass after all of US3/US4/US5 (T028–T032, T044–T053,
  T064–T074) are complete.
- [ ] T078 Run the full project test suite one final time —
  `ctest --test-dir build -L visual --output-on-failure`,
  `ctest --test-dir build -L libshader --output-on-failure`,
  `ctest --test-dir build -L shading_parity --output-on-failure` — and
  confirm 100% passing, matching issue #1's established verification bar
  (spec.md SC-001 through SC-007).
- [ ] T079 Update `DEVNOTES.md`'s JIT status row/Open Issues (matching
  the entries issue #1's fix and spec 011/012/014 each added) to record
  this feature's completion, and update `CLAUDE.md`'s "Known gotchas" #12
  (`random()`/`urandom()` under the JIT) and the "In-progress work" note
  this plan's Phase 1 added, reflecting that the broader builtin-function
  gap (all 43 functions, including the 18 found mid-implementation) is
  now closed.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — confirms already-completed
  groundwork.
- **Foundational (Phase 2)**: Depends on Setup; intentionally minimal (no
  new shared infrastructure needed, `research.md` D2).
- **User Story 1 (Phase 3)**: Depends on Foundational. No dependency on
  any other user story.
- **User Story 2 (Phase 4)**: **Hard dependency on User Story 1 being
  fully complete** (T010–T017) — not just priority ordering. See
  `contracts/gate-hardening-contract.md`'s Sequencing Requirement.
- **User Story 3 (Phase 5)**: Depends on Foundational only. No hard
  dependency on US1 or US2 — may run in parallel with US2, or before it,
  if preferred. Presented after US2 to match spec.md's priority order.
- **User Story 4 (Phase 6)**: Depends on Foundational only. Same
  no-hard-dependency note as US3.
- **User Story 5 (Phase 7)**: Depends on Foundational only, plus (Group C
  only, T067/T068) on User Story 1's `jitOcclusionBatch` architecture
  already existing (it does — US1 is complete). No hard dependency on
  US2/US3/US4 — may run in parallel with any of them. Discovered
  mid-implementation (during US2); presented after US4 since it was found
  later, not because it is lower priority than US3/US4.
- **Polish (Phase 8)**: Depends on US1–US5 all being complete (T077/T078
  specifically require US3+US4+US5 done).

### Within Each User Story

- Tests MUST be written and confirmed failing before that function's
  implementation task (constitution Principle III, enforced per-function
  in T004–T009/T023–T027/T034–T043/T055–T063, not just once per story).
- Implementation tasks touching the same shared files
  (`llvmEmitter.cpp`, `rslOps.cpp`/`.h`, `shading.h`/`.cpp`) are NOT marked
  `[P]` even across different functions within a story — sequential edits
  to the same files avoid merge conflicts. Test tasks creating new,
  distinct probe/RIB files ARE marked `[P]`.

### Parallel Opportunities

- All test tasks within a story (T004–T009, T023–T027, T034–T043,
  T055–T063) can run in parallel — each creates new, distinct files.
- User Stories 3, 4, and 5 have no hard dependency on each other or on
  User Story 2 — a team could work US1, US3, US4, and US5 simultaneously
  after Foundational, converging on US2 only once US1 is done (and
  ideally after US3/US4/US5 too, so the coverage guard's red-state window
  in Phase 4 is as short as possible — see T021's note).

---

## Parallel Example: User Story 1 tests

```bash
# Launch all six probe/RIB/reference creation tasks together:
Task: "Create shaders/visibility_probe.sl + paired RIB scenes + reference + ctest registration"
Task: "Create shaders/transmission_probe.sl + paired RIB scenes + reference + ctest registration"
Task: "Create shaders/trace_probe.sl + paired RIB scenes + reference + ctest registration"
Task: "Create shaders/occlusion_probe.sl + paired RIB scenes + reference + ctest registration"
Task: "Create shaders/indirectdiffuse_probe.sl + paired RIB scenes + reference + ctest registration"
Task: "Create shaders/comp_probe.sl + paired RIB scenes + reference + ctest registration"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational
3. Complete Phase 3: User Story 1
4. **STOP and VALIDATE**: `quadlight.rib`/`spherelight.rib` render
   correctly under `--jit` — the actual "JIT show-stopper" from issue #3
   is closed. This alone is a complete, demonstrable, shippable increment.

### Incremental Delivery (recommended order, matching spec.md priorities)

1. Setup + Foundational → baseline confirmed green
2. User Story 1 → validate independently → **MVP**: the show-stopper is
   fixed
3. User Story 2 → validate independently → the defect *class* is closed
   (gate hardened; coverage guard red for US3/US4/US5 until they land —
   expected; US5 itself was discovered by running this guard)
4. User Story 3 → validate independently
5. User Story 4 → validate independently
6. User Story 5 → validate independently → coverage guard reaches full
   green (T075/T077)
7. Polish → final full-suite confirmation, doc updates

### Alternative: Maximize Parallelism

Since US3, US4, and US5 have no hard dependency on US1 or US2 (only on
Foundational, and US5's Group C on US1's already-complete point-cloud
architecture), a team with capacity could run US1, US3, US4, and US5
concurrently right after Foundational, then converge on US2 last —
shortening the coverage guard's expected red-state window (T021's note)
to as little as possible, ideally zero if US3/US4/US5 all land before
US2's T021.

---

## Notes

- `[P]` tasks = different files, no dependency on an incomplete task.
- `[Story]` label maps every user-story-phase task to its story for
  traceability; Setup/Foundational/Polish tasks carry no story label per
  the template convention.
- Per the user's standing process instructions for this whole feature: no
  task in this file should be auto-committed — commits are the user's
  manual intervention throughout implementation, and `speckit.analyze`
  must run (and be reviewed) before `speckit.implement` begins.
