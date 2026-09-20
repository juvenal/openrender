# Tasks: LLVM JIT Builtin-Function Coverage

**Input**: Design documents from `/specs/017-jit-builtin-function-coverage/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md (all present)

**Tests**: Included and REQUIRED — spec.md's FR-018 mandates a persisted
regression test per function, and the Constitution Check (plan.md) carries
an explicit TDD sequencing gate: every function's probe shader + RIB pair
must exist and be confirmed failing *before* that function's engine fix
lands.

**Organization**: Tasks are grouped by user story (US1–US4, matching
spec.md's priorities P1/P1/P2/P3) to enable independent implementation and
testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an
  incomplete task)
- **[Story]**: Which user story this task belongs to (US1/US2/US3/US4)
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

- [ ] T018 [US2] Write a new `ctest` (shells out to `oshader --jit` against
  a fixture `.sl` calling a deliberately-unhandled or synthetic/nonexistent
  builtin mnemonic — see `contracts/gate-hardening-contract.md`'s
  Verification Obligations #2) under `src/libshader/tests/` (or a small new
  test file registered alongside `test_opcode_coverage.cpp`), asserting
  nonzero exit + a diagnostic naming that mnemonic + no `.slo` produced.
  Confirm it currently FAILS (gate not yet hardened — `oshader --jit`
  still exits 0 and writes a `.slo` today).

### Implementation for User Story 2

- [ ] T019 [US2] Harden the coverage gate: change
  `src/libshader/compiler/llvmEmitter.cpp`'s `emitFunction()` gate (line
  826, `if (!isHandledOpcode(op)) continue;`) to instead record a hard
  failure naming the specific unhandled mnemonic, propagated through
  `emitFunction()`'s return and `emitLLVMBitcode()`'s existing `bool`
  return — `oshader.cpp:431-435`'s `ERR_COMPILE` exit path needs no change
  (`contracts/gate-hardening-contract.md`). Confirm T018 now passes.
- [ ] T020 [US2] Confirm zero build breakage: `rm -rf build && cmake -B build -S . && cmake --build build --config Release`
  from a clean tree succeeds with zero new failures (FR-009/SC-004;
  `contracts/gate-hardening-contract.md`'s Verification Obligations #1).
- [ ] T021 [US2] Add `kAllFunctionMnemonics[]` to
  `src/libshader/compiler/llvmEmitter.h`/`.cpp`, generated via the
  `kOpcodeParamTable` X-macro technique filtered to `scriptFunctions.h`'s
  `#include` chain (`research.md` D3), hand-excluding the two `"XXX"`
  DSO-placeholder rows (D7). Add the coverage-guard test to
  `src/libshader/tests/test_opcode_coverage.cpp` asserting
  `kAllFunctionMnemonics ⊆ kHandledOpcodes`, naming any missing mnemonic on
  failure (`contracts/function-coverage-guard-contract.md`). **Expected,
  correct result at this point**: `ctest -L libshader` now fails, by name,
  for every one of User Story 3/4's 19 not-yet-implemented functions —
  this is the guard working as designed (FR-010), not a regression; it
  reaches green state only once US3 and US4 both complete (see T056).
- [ ] T022 [US2] Remove the narrow, hand-written `random`/`urandom`-only
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

**Checkpoint**: All 25 functions from issue #3's inventory are now
JIT-handled. `ctest -L libshader`'s coverage guard (T021) should now be
fully green for the first time since it was introduced in US2 — this is
the guard's intended green-state finalization (mirrors spec 011's own
precedent for its coverage guard).

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Final validation and small cleanups spanning multiple
stories.

- [ ] T055 [P] Correct `shaders/usfroma_probe.sl`'s header comment — it
  claims `Oi = 1;` is required "because the JIT does not default Oi to
  opaque," which `research.md` D6 confirmed is stale (fixed by spec 014).
- [ ] T056 Confirm `ctest -L libshader`'s `kAllFunctionMnemonics` guard
  (T021) is now fully green — the green-state finalization flagged in
  T021's note, now that US3 (T028–T032) and US4 (T044–T053) are both
  complete.
- [ ] T057 Run the full project test suite one final time —
  `ctest --test-dir build -L visual --output-on-failure`,
  `ctest --test-dir build -L libshader --output-on-failure`,
  `ctest --test-dir build -L shading_parity --output-on-failure` — and
  confirm 100% passing, matching issue #1's established verification bar
  (spec.md SC-001 through SC-005).
- [ ] T058 Update `DEVNOTES.md`'s JIT status row/Open Issues (matching
  the entries issue #1's fix and spec 011/012/014 each added) to record
  this feature's completion, and update `CLAUDE.md`'s "Known gotchas" #12
  (`random()`/`urandom()` under the JIT) and the "In-progress work" note
  this plan's Phase 1 added, reflecting that the broader builtin-function
  gap is now closed.

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
- **Polish (Phase 7)**: Depends on US1–US4 all being complete (T056/T057
  specifically require US3+US4 done).

### Within Each User Story

- Tests MUST be written and confirmed failing before that function's
  implementation task (constitution Principle III, enforced per-function
  in T004–T009/T023–T027/T034–T043, not just once per story).
- Implementation tasks touching the same shared files
  (`llvmEmitter.cpp`, `rslOps.cpp`/`.h`, `shading.h`/`.cpp`) are NOT marked
  `[P]` even across different functions within a story — sequential edits
  to the same files avoid merge conflicts. Test tasks creating new,
  distinct probe/RIB files ARE marked `[P]`.

### Parallel Opportunities

- All test tasks within a story (T004–T009, T023–T027, T034–T043) can run
  in parallel — each creates new, distinct files.
- User Stories 3 and 4 have no hard dependency on each other or on User
  Story 2 — a team could work US1, US3, and US4 simultaneously after
  Foundational, converging on US2 only once US1 is done (and ideally
  after US3/US4 too, so the coverage guard's red-state window in Phase 4
  is as short as possible — see T021's note).

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
   (gate hardened; coverage guard red for US3/US4 until they land — expected)
4. User Story 3 → validate independently
5. User Story 4 → validate independently → coverage guard reaches full
   green (T056)
6. Polish → final full-suite confirmation, doc updates

### Alternative: Maximize Parallelism

Since US3 and US4 have no hard dependency on US1 or US2 (only on
Foundational), a team with capacity could run US1, US3, and US4
concurrently right after Foundational, then converge on US2 last —
shortening the coverage guard's expected red-state window (T021's note)
to as little as possible, ideally zero if US3/US4 both land before US2's
T021.

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
