# Phase 1 Data Model: LLVM JIT Builtin-Function Coverage

This feature has no persistent storage or user-facing data model. The
"entities" below are the in-process/build-time structures this feature
introduces or manipulates, derived from spec.md's Key Entities section.

## RSL Builtin Function

A named, callable RenderMan Shading Language function (as distinct from a
bytecode-level operator) that the compiler lowers into an intermediate
call instruction consumed by both backends.

| Field | Type | Source of truth | Notes |
|---|---|---|---|
| `mnemonic` | string | `DEFFUNC`/`DEFLINKFUNC`/`DEFSHORTFUNC`/`DEFLIGHTFUNC` `text` field in `scriptFunctions.h`/`giFunctions.h` | Multiple overloads (e.g. `Randomf`/`Randomv`) can share one mnemonic, disambiguated by prototype and/or destination stride at codegen time, not by mnemonic. |
| `prototype(s)` | string(s) | Same `DEF*FUNC` declaration's second string argument | e.g. `"f=pp!"` for `visibility` — return type, operand types, optional trailing `!` for the (unsupported-under-JIT, see spec.md Edge Cases) named-argument extension. |
| `dispatch_family` | enum | Macro naming convention | `DEFFUNC` (loops full `numVertices`), `DEFSHORTFUNC` (loops `numRealVertices` only — see D1), `DEFLINKFUNC` (link-alias, no own codegen), `DEFLIGHTFUNC` (light-shader-only). Determines the correctness discipline a JIT wrapper must follow. |
| `interpreter_body` | macro reference | The `*EXPR`/`*EXPR_PRE`/`*EXPR_UPDATE`/`*EXPR_POST` macro family the dispatch macro expands to | What a JIT wrapper must delegate to/transcribe per FR-016. |
| `tier` | enum | This feature's spec.md priorities | P1 (US1, 6 functions), P2 (US3, 5 functions), P3 (US4, 14 functions). |
| `shipped_callers` | file list | Confirmed by grep across `shaders/*.sl` | Non-empty only for US1's six; drives gate-hardening sequencing (D4). |

## Coverage Gate (`kHandledOpcodes[]`)

The JIT compiler's existing single point of dispatch that decides, per
instruction, whether JIT code is emitted for it (`llvmEmitter.cpp:62-90`,
checked via `isHandledOpcode()` at the gate in `emitFunction()`,
`llvmEmitter.cpp:826`). This feature extends it (new mnemonics added per
function fixed) and, in US2, hardens its failure mode.

| Field | Type | Notes |
|---|---|---|
| `mnemonic` | `const char*` | Matches `RSL Builtin Function.mnemonic`. |
| (pre-US2 behavior) | `continue` (silent skip) | Zero IR emitted for a call to an unlisted mnemonic — the defect this whole feature (and issue #1) exists to close. |
| (post-US2 behavior) | hard failure, propagated to `emitLLVMBitcode()`'s `bool` return | `oshader --jit` exits nonzero with a diagnostic naming the specific unhandled mnemonic (FR-008). |

## Coverage Guard (`kAllFunctionMnemonics[]`, new in US2)

The automated, test-suite-time check (mirroring the existing
`kAllOpcodeMnemonics`/`test_opcode_coverage.cpp` guard, extended per D3 to
the `FUNCTION_` family) that fails `ctest -L libshader` if any mnemonic the
compiler can actually produce is missing from the Coverage Gate.

| Field | Type | Notes |
|---|---|---|
| `mnemonic` | `const char*` | Generated via the `kOpcodeParamTable` X-macro technique (D3), filtered to `scriptFunctions.h`'s `#include` chain, `"XXX"`'s two rows hand-excluded (D7). |
| **Invariant enforced** | — | For every `RSL Builtin Function` with a nonempty `interpreter_body`, its `mnemonic` MUST appear in `kHandledOpcodes[]`. Violation fails `ctest -L libshader` naming the specific missing mnemonic, re-derived at test-run time (never a list frozen at feature end). |

## `op_*` JIT Wrapper (new instances, `rslOps.h`/`.cpp`)

Each new JIT-callable delegation trampoline this feature adds.

| Field | Type | Notes |
|---|---|---|
| `name` | string | `op_<mnemonic>` convention (e.g. `op_visibility`), or `op_<mnemonic>_<variant>` where one mnemonic has multiple codegen forms (`op_trace_f`/`op_trace_c`, `op_comp`/`op_mcomp`, `op_min_f`/`op_min_v`). |
| `signature` | C-linkage function signature | Follows the operand shape of the `RSL Builtin Function` it implements. Pure-math functions (US4) take only their float/vector/matrix/string operands + `n`/`tags`; raytracing-tier functions (US1/US3's `photonmap`) additionally take `du`/`dv`/`N`/`time` grid pointers (see `contracts/op-wrapper-abi.md`). |
| `needs_context` | boolean | True for every US1 raytracing-tier function, `photonmap`, `rayinfo`/`raylabel`/`raydepth` (all reach `CShadingContext` state via `libshader::activeContext()`); false for `comp`/`ptlined`/all of US4 (pure operand functions, matching `op_reflect`'s existing pattern of not needing context). |
| `delegates_to` | reference | The `RSL Builtin Function.interpreter_body`'s target — a `mathSpec.h` free function (US4), a new `CShadingContext` member method transcribing a `giFunctions.h` macro family (US1's raytracing tier, `photonmap`), or a direct alias to an existing `op_*` (`match`→`op_seql`). |

## New `CShadingContext` Member Methods (`shading.h`/`.cpp`, US1 + `rayinfo`)

| Method | Delegates to (interpreter macro family) | Uses private `CTraceLocation`/`traceTransmission`/`traceReflection`? |
|---|---|---|
| `jitVisibility` | `TRANSMISSIONEXPR*` + `VISIBILITYEXPR_POST` | Yes |
| `jitTransmission` | `TRANSMISSIONEXPR*` + `TRANSMISSIONEXPR_POST` | Yes |
| `jitTrace` (float + color forms) | `TRACEEXPR*` (= `TRANSMISSIONEXPR*`) + `TRACE2EXPR_POST`/`TRACEEXPR_POST` | Yes |
| `jitOcclusion` | `IDEXPR*` | No — uses `rendererGetCache`/`rendererGetTexture3d`/`rendererGetEnvironment` (protected) instead |
| `jitIndirectDiffuse` | `IDEXPR*` | No — same as `jitOcclusion` |
| `jitRayInfo` | `RAYINFOEXPR*` | No — reads private `currentRayDepth`/`currentRayLabel` + `varying[VARIABLE_P/I]` |

All are new `public` methods, added alongside the existing `jitShadowF`
(`shading.h:501`) — no access-level change to any existing member.

## Regression Test Pair (new, per function fixed — FR-018)

| Field | Type | Notes |
|---|---|---|
| `probe_shader` | `shaders/<name>_probe.sl` | Writes the function's result into `Ci`. No `Oi = 1;` workaround needed (D6). |
| `rslo_scene` | `examples/rib/tests/sphere-<name>-reyes.rib` | Interpreter backend; generates the reference `.tif`. |
| `slo_scene` | `examples/rib/tests/sphere-<name>-reyes-slo.rib` | `Attribute "shade" "shaderformat" ["slo"]`; diffed against the same reference. |
| `numthreads_1` | boolean | Required (+ explanatory comment) for every probe whose output is stochastic — all of US1's raytracing tier + `photonmap` (confirmed thread-scheduling-sensitive during issue #1's `random()` work: two default-multithreaded same-scene interpreter renders differed by MaxBlockAvgDiff 163; forced to `numthreads 1`, diff 0.00). |
| `ctest_registration` | `tests/visual/CMakeLists.txt` entry pair | `Visual_sphere-<name>-reyes` / `Visual_sphere-<name>-reyes-slo`, both pointed at the same reference `.tif`, following the `sphere-random-reyes{,-slo}` precedent exactly. |

## Show-Stopper Closure Pair (new, US1 only — FR-019)

| Field | Type | Notes |
|---|---|---|
| `scene` | `examples/rib/quadlight.rib` / `examples/rib/spherelight.rib` | Existing example scenes, currently unregistered in `tests/visual/CMakeLists.txt` — confirmed via grep. |
| `registration` | New permanent `ctest -L visual` pair per scene | Not a one-time manual check (per spec.md Clarifications) — the concrete, CI-enforced closure criterion for issue #1's original repro. |
