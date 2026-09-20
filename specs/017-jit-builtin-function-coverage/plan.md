# Implementation Plan: LLVM JIT Builtin-Function Coverage

**Branch**: `017-jit-builtin-function-coverage` | **Date**: 2026-09-20 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/017-jit-builtin-function-coverage/spec.md`

**Note**: This template is filled in by the `/speckit-plan` command; its definition describes the execution workflow.

## Summary

The `.slo` (LLVM JIT) shading backend's `emitFunction()` (`llvmEmitter.cpp:509`)
gates every IR instruction against a hand-maintained `kHandledOpcodes[]`
allowlist (lines 62-90) via `isHandledOpcode()` (lines 92-98), at the gate
`if (!isHandledOpcode(op)) continue;` (line 826). Anything not listed is
silently skipped: zero LLVM IR emitted, no error, destination buffer never
written. This is the exact mechanism that caused issue #1
(`random()`/`urandom()`, fixed and merged) and, per issue #3, still affects
25 more RSL builtin functions — including `visibility()`/`transmission()`,
which is what actually blocks issue #1's own repro scenes
(`quadlight.rib`/`spherelight.rib`) from rendering correctly under `--jit`.

Technical approach, phased by spec.md's four user stories:

1. **US1 (P1, 6 functions)**: implement `visibility`, `transmission`,
   `trace`, `occlusion`, `indirectdiffuse` — the first JIT-compiled code in
   this codebase's history to construct and consume an actual traced-ray
   batch or non-deterministic cache lookup (every existing JIT builtin that
   superficially resembles them — `shadow`, `texture`, `environment` — is
   in fact a deterministic position→value lookup with zero `CTraceLocation`
   involvement) — plus `comp` (trivial, bundled in purely for its real
   shipped-shader impact). Every raytracing-tier wrapper must replicate the
   interpreter's `numRealVertices`-bounded-then-replicate discipline
   exactly (see Technical Context "Constraints" and `research.md` D1) to
   avoid both silently wrong derivative output and ~3× wasted ray tracing.
2. **US2 (P1)**: harden the silent-skip gate into a hard `oshader --jit`
   compile-time error, and extend `test_opcode_coverage.cpp`'s guard from
   `OPCODE_`-only to the full `FUNCTION_`-family builtin mnemonic set —
   reusing `kOpcodeParamTable`'s existing `#include`-based X-macro
   mechanism (`llvmEmitter.cpp:156-177`), which already walks
   `scriptFunctions.h`/`giFunctions.h`. Sequenced after US1 specifically:
   confirmed by direct grep that zero shipped shaders reference any
   function outside US1's six once US1 lands, so hardening then causes no
   build breakage.
3. **US3 (P2, 5 functions)**: `photonmap`, `rayinfo`, `raylabel`,
   `raydepth`, `ptlined` — no shipped callers today, meaningfully simpler
   than US1 (single kd-tree lookup, private-field reads, or pure geometry).
4. **US4 (P3, 14 functions)**: pure math/string functions, each a small
   mechanical `op_*` wrapper copying an already-shipped sibling template;
   every needed math primitive already exists in `src/common/mathSpec.h`.

Every function fixed, in every story, gets its own persisted `.slo`-vs-`.rslo`
regression test at implementation time (FR-018), following the
`shaders/random_probe.sl` + `examples/rib/tests/sphere-random-reyes{,-slo}.rib`
template from issue #1's fix. `examples/rib/quadlight.rib`/`spherelight.rib`
become permanent registered regression pairs too (FR-019), not one-time
manual checks.

## Technical Context

**Language/Version**: C++20 (constitution Principle II) for all new/modified
code; `op_*` JIT-callable wrappers in `rslOps.cpp` keep C linkage (existing
convention, `src/libshader/shading/rslOps.h`).

**Primary Dependencies**: LLVM (ORC JIT, IR emission — same components
`src/libshader/shading/CMakeLists.txt` already requires). No new external
dependency introduced. As of this feature's planning, the project's macOS
local-build toolchain switched to vendoring LLVM/PNG/TIFF/zlib/OpenEXR via
vcpkg exclusively (see `research.md` D8) rather than Homebrew — a build
environment decision, not a new dependency; LLVM's vendored version
(23.1.1) is unchanged from what development already used.

**Storage**: N/A — `.rslo`/`.slo` are compiled shader files on disk, not a
database; no schema/storage layer touched.

**Testing**: `ctest`, using this project's existing label conventions:
`-L visual` (8×8 block-average image-diff regression — new `-slo` probe
pairs added per fixed function, plus the newly-registered
`quadlight`/`spherelight` pairs) and `-L libshader` (compiler/runtime unit
tests — the extended `FUNCTION_`-mnemonic coverage guard, and a new
negative test proving the hardened gate fires). No new ctest label needed
(unlike spec 011's `perf-manual`) since this feature carries no performance
requirement (see Performance Goals below).

**Target Platform**: Linux and macOS (constitution Principle VI). New
`op_*` symbols are subject to the macOS JIT dead-stripping gotcha
(`CLAUDE.md` #3) — each must be confirmed to resolve at JIT bind time via
the existing `DynamicLibrarySearchGenerator`, per the same verify-on-add
discipline spec 011 established (no blanket `jitSymbolRetain.cpp`
prerequisite).

**Project Type**: Not a standalone app/service — a correctness-parity fix
spanning two existing libraries within the single `orender` C++ CLI
renderer: the compiler frontend (`libshader_compiler` — `llvmEmitter.cpp`,
`opcodes.cpp`) and the runtime shading engine (`libshader_shading` —
`rslOps.cpp`, `shading.h`/`.cpp`). No new executable, service, or public
interface is introduced.

**Performance Goals**: None — per spec.md's Clarifications, this feature
carries no wall-clock performance requirement or success criterion.
Correctness parity with the interpreter (FR-001 through FR-016) is the
only bar; spec 011's equivalent bar (JIT ≥10% faster than interpreter) is
documented in `DEVNOTES.md` as never actually met project-wide, so it is
not repeated here. FR-007's `numRealVertices`-bounded-then-replicate
discipline is a *correctness* requirement (matching interpreter behavior
exactly), which happens to also avoid ~3× redundant ray tracing as a side
effect — not a benchmarked target in its own right.

**Constraints**:
- FR-007/FR-011 (the `numRealVertices` derivative-replication discipline):
  every `DEFSHORTFUNC`-based wrapper (`visibility`, `transmission`,
  `trace`×2, `occlusion`, `indirectdiffuse`, `photonmap`×2, `rayinfo` — 9 of
  the 10 P1+P2 functions) MUST evaluate its underlying trace/cache-lookup
  operation only once per real shading point (`currentShadingState->numRealVertices`,
  not the full `numVerts` argument the LLVM-generated caller passes), then
  copy that single result into the corresponding derivative-offset
  destination slots — never re-invoke the stochastic operation for those
  slots. This mirrors `execute.cpp`'s `expandVector`/`expandFloat` macros
  (lines 356-377) exactly, and is the same class of correctness trap spec
  011's `gather()`/`gatherElse()`/`gatherEnd()` fix already encountered
  (`gatherSample`, `shading.cpp:2711-2712`, reading `numRealVertices`
  rather than trusting the passed vertex count). `raylabel`/`raydepth` are
  exempt from the replication step (interpreter leaves those tail slots
  unwritten too — not meaningfully differentiable quantities). `ptlined`
  (the one plain `DEFFUNC` in this group) needs no special treatment at
  all — loop the full `numVerts` like any already-handled math opcode.
- FR-016 (delegation-only): every JIT fix must compute its result by
  invoking the same underlying implementation (host free function,
  `CShadingContext` method, or shared math primitive) the interpreter
  already uses for that function — never new shading math independently in
  the JIT path. For the raytracing tier specifically, this means
  transcribing the corresponding `giFunctions.h` macro family
  byte-faithfully inside a new `CShadingContext` member method (see
  `research.md` D2/D3), not approximating it.
- FR-008/FR-009 (gate-hardening sequencing): the compile-time hardening of
  `isHandledOpcode()`'s gate MUST NOT be enabled until every builtin
  function any currently-shipped shader calls (US1's six) has JIT support
  — confirmed by direct grep this is exactly and only US1's six; hardening
  strictly after US1 (as its own last step) causes zero build breakage,
  both immediately and for the whole duration US3/US4 remain unimplemented.
- FR-017: the interpreter (`.rslo`) remains the reference implementation
  throughout and its behavior MUST NOT change — including reproducing,
  not "fixing", `match()`'s plain-string-equality semantics
  (`scriptFunctions.h:662`'s own `// FIXME: Subpattern matching is not
  implemented yet`), `round()`'s truncating-cast semantics, and
  `min()`/`max()`'s shared two-argument-only support.

**Scale/Scope**: Bounded by spec.md's confirmed 25-function inventory
(6 + 5 + 14 = 25, `comp` counted once, in US1) plus the two build-infrastructure
deliverables (gate-hardening, coverage-guard extension) and the two
newly-registered example-scene regression pairs (`quadlight`/`spherelight`).
No multi-repo or multi-service scope. `XXX` (issue #3's original list)
confirmed NOT a 27th function — see `research.md` D7.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Gate | Status |
|---|---|---|
| I. Clean Code | Small, focused functions; no magic numbers/deep nesting | **PASS** — new `op_*` wrappers in `rslOps.cpp` are single-responsibility trampolines (see `contracts/op-wrapper-abi.md`); the raytracing-tier `jitXxx` methods in `shading.cpp`, while genuinely new code, are structured as direct macro-family transcriptions with named constants, not novel logic. |
| II. Language Standards | C++20/C17, no non-standard extensions | **PASS** — matches existing `rslOps.h`/`.cpp` C-linkage convention; no new language surface. |
| III. Test-Driven Development (NON-NEGOTIABLE) | Red→Green→Refactor; tests before implementation | **PASS, with a sequencing requirement carried into `/speckit-tasks`** (mirrors spec 011's precedent exactly): for every function, the probe shader + paired RIB scenes + reference image (FR-018) must exist and be confirmed to fail (silently-wrong or mismatched output) *before* that function's `llvmEmitter.cpp`/`rslOps.cpp`/`shading.h`/`.cpp` fix lands — tasks.md must not reorder this per-function. The gate-hardening negative test (proving `oshader --jit` fails loudly on an unhandled mnemonic) must similarly be written and shown to fail against the *pre-hardening* gate before the hardening change lands. |
| IV. Command Line Interface | Functionality via CLI, stdin/stdout/stderr | **PASS (no new surface)** — no new CLI added; existing `oshader --jit`/`orender` behavior unchanged except for the functions this feature fixes, and `oshader --jit` gains a new *failure* mode (US2) that is itself a stderr diagnostic + nonzero exit, consistent with existing CLI exit-code conventions. |
| V. Minimal Dependencies | No new dependency without justification | **PASS** — zero new external dependencies; LLVM was already required, vcpkg vendoring is a build-environment change already merged to master prior to this feature, not introduced by it. |
| VI. Platform Targeting | Linux/macOS only | **PASS** — no platform-specific code added beyond the existing JIT dead-stripping mitigation pattern (`CLAUDE.md` #3), applied per new symbol as needed. |
| VII. Documentation and Site Management | Docs kept in sync | **PASS** — this is an internal correctness fix with no new user-visible product functionality, matching spec 011's precedent for the same reasoning (spec.md's Assumptions); no new Hugo `site/` page required. `usfroma_probe.sl`'s now-stale `Oi = 1;` comment (see `research.md` D6) is noted for correction as a small doc-drift fix, mirroring spec 011's FR-010 precedent. |

No violations requiring Complexity Tracking justification. The one
genuinely new structural element — brand-new `CTraceLocation`-batch JIT
machinery for the raytracing tier, with no existing JIT precedent to
extend — is accepted complexity inherent to the feature's actual scope
(confirmed via investigation that no simpler existing pattern applies),
not avoidable complexity; it is scoped to five new `CShadingContext`
member methods (`jitVisibility`/`jitTransmission`/`jitTrace`/`jitOcclusion`/
`jitIndirectDiffuse`) that each transcribe exactly one interpreter macro
family, kept as small and focused as the macro family they mirror.

## Project Structure

### Documentation (this feature)

```text
specs/017-jit-builtin-function-coverage/
├── plan.md              # This file (/speckit-plan command output)
├── research.md          # Phase 0 output (/speckit-plan command)
├── data-model.md        # Phase 1 output (/speckit-plan command)
├── quickstart.md        # Phase 1 output (/speckit-plan command)
├── contracts/           # Phase 1 output (/speckit-plan command)
│   ├── op-wrapper-abi.md
│   ├── function-coverage-guard-contract.md
│   └── gate-hardening-contract.md
└── tasks.md             # Phase 2 output (/speckit-tasks command - NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
src/
├── libshader/
│   ├── compiler/                       # libshader_compiler — .sl -> .rslo/.slo
│   │   ├── llvmEmitter.cpp               # emitFunction(): new visibility/transmission/trace/
│   │   │                                 #   occlusion/indirectdiffuse/comp + all US3/US4 op==
│   │   │                                 #   branches; kHandledOpcodes[] additions; kAllFunctionMnemonics[]
│   │   │                                 #   (new, X-macro over scriptFunctions.h, mirrors kOpcodeParamTable);
│   │   │                                 #   gate-hardening: isHandledOpcode() false -> hard error path
│   │   ├── llvmEmitter.h                 # kAllFunctionMnemonics[] declaration (new, alongside kHandledOpcodes)
│   │   └── opcodes.cpp / opcodes.h       # (reference only — kAllOpcodeMnemonics is the OPCODE_-only
│   │                                     #   template kAllFunctionMnemonics mirrors; not itself modified)
│   │
│   └── shading/                          # libshader_shading — .rslo/.slo runtime execution
│       ├── rslOps.h / rslOps.cpp         # new op_visibility/op_transmission/op_trace_f/op_trace_c/
│       │                                 #   op_occlusion/op_indirectdiffuse/op_comp/op_mcomp (US1);
│       │                                 #   op_photonmap*/op_rayinfo/op_raylabel/op_raydepth/op_ptlined (US3);
│       │                                 #   op_degrees/op_determinant/op_distance/op_min*/op_refract/
│       │                                 #   op_rotate*/op_round/op_scale/op_setcomp/op_step/op_translate/
│       │                                 #   op_concat/op_format (US4) — all thin free-function trampolines
│       │                                 #   via libshader::activeContext(), matching op_shadow_f's shape
│       ├── shading.h                     # new public CShadingContext methods: jitVisibility/jitTransmission/
│       │                                 #   jitTrace/jitOcclusion/jitIndirectDiffuse (US1, alongside jitShadowF
│       │                                 #   at line 501); jitRayInfo (US3, new private-field access)
│       └── shading.cpp                   # new method bodies — genuinely new C++ transcribing giFunctions.h's
│                                         #   TRANSMISSIONEXPR*/TRACEEXPR*/IDEXPR* macro families (US1) and
│                                         #   PHOTONMAPEXPR*/RAYINFOEXPR* (US3)
│
├── oshader/oshader.cpp                   # US2: no change expected — existing ERR_COMPILE path (line 431-435)
│                                         #   already handles emitLLVMBitcode() returning false
│
tests/
└── libshader/                            # (repo-root src/libshader/tests/, not tests/libshader/)
    └── test_opcode_coverage.cpp          # US2: new test asserting kAllFunctionMnemonics coverage,
                                         #   superseding issue #1's narrow random/urandom-only check;
                                         #   new negative test proving hardened oshader --jit fails loudly

shaders/                                  # new probe shaders, one (or one per tightly-related group) per
                                         #   function fixed — visibility_probe.sl, transmission_probe.sl,
                                         #   trace_probe.sl, occlusion_probe.sl, indirectdiffuse_probe.sl,
                                         #   comp_probe.sl (US1); photonmap_probe.sl, rayinfo_probe.sl,
                                         #   raylabel_probe.sl, raydepth_probe.sl, ptlined_probe.sl (US3);
                                         #   one or a few grouped probes for US4's 14 functions

examples/rib/tests/                       # matching RIB scene pairs per probe (sphere-<name>-reyes{,-slo}.rib),
                                         #   following sphere-random-reyes{,-slo}.rib exactly; numthreads 1 +
                                         #   explanatory comment for every stochastic-output probe (all of
                                         #   US1's raytracing tier + photonmap)
examples/rib/tests/references/            # new reference .tif per probe pair

examples/rib/                             # quadlight.rib/spherelight.rib (existing, unmodified content — no
                                         #   reference .tif exists for either yet; this feature generates the
                                         #   first one). examples/rib/tests/ gains a new -slo sibling scene
                                         #   each (quadlight-slo.rib/spherelight-slo.rib), a copy with
                                         #   Attribute "shade" "shaderformat" ["slo"] added — see
                                         #   data-model.md's Show-Stopper Closure Pair entity

tests/visual/CMakeLists.txt               # new add_visual_test() registrations for every probe pair (US1/US3/US4)
                                         #   and for quadlight/spherelight (US1, FR-019)
```

**Structure Decision**: Single existing project (the `orender` C++ CLI
renderer and its libraries) — no new top-level directory or build target.
Every change lands inside the two existing libraries
(`libshader_compiler`, `libshader_shading`) plus the existing test/example
scaffolding (`shaders/`, `examples/rib/tests/`, `tests/visual/CMakeLists.txt`,
`src/libshader/tests/`), following the exact file layout issue #1's fix and
spec 011 already established for this class of work.

## Complexity Tracking

*No entries — Constitution Check found no violations requiring
justification (see above).*
