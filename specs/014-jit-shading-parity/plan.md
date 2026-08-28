# Implementation Plan: JIT/Interpreter Shading Parity Fixes

**Branch**: `014-jit-shading-parity` | **Date**: 2026-08-27 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/014-jit-shading-parity/spec.md`

**Note**: This template is filled in by the `/speckit-plan` command; its definition describes the execution workflow.

## Summary

The JIT (`.slo`) shading backend computes its `usedParameters` bitmask from
the compiler's pre-seeded global-variable table (`CScriptContext`
unconditionally seeds ~26 RSL built-in globals before parsing), not from
actual references in the shader's compiled instructions — so the bitmask is
a near-constant true for every shader regardless of content. This breaks the
shared runtime's `Ci`/`Oi` default-fill fallback for JIT shaders (real
garbage/black-pixel bug), leaves the raytrace and message-passing bits
permanently unset (JIT never sets either, since neither maps to a global
variable name the current name-based scan can see), and makes the
non-ambient-lighting bit's narrow local scan wrong (it treats `illuminance`
as if it were `illuminate`/`solar`). Separately, an unrelated pair of
ambient-`Cl` accumulation bugs exist: the interpreter's `execEnd:` block is
missing a null-guard the JIT path already has, and `prepareAmbient()`
double-counts a light's ambient contribution.

The technical approach: (1) replace the compiler's declared-scope/
`SLC_GLOBAL`-gated scan with a single pass over IR instructions that matches
actual variable references (`result` and `operands`) against a name→bit
table, fixing the global-variable half of `usedParameters`; (2) extend that
same pass with an opcode/function→bit table re-expanded from the
interpreter's own `shaderOpcodes.h`/`shaderFunctions.h`/`giOpcodes.h`/
`giFunctions.h` via X-macro re-inclusion (the proven pattern already used in
`src/libshader/shading/rslo_code.h`), rather than a second hand-transcribed
table — this single combined mechanism is what fixes raytrace,
message-passing, non-ambient, and the derivative family together, and is
what makes the family's opcode-triggered half (e.g. `texture()` implying
derivative use without a literal `du`/`dv` token) correct instead of
accidentally regressed; (3) add the two narrowly-scoped interpreter fixes
(null-guard, remove the duplicate accumulation) with no interpreter behavior
change beyond those two spots, since the interpreter is this feature's
reference implementation; (4) investigate and resolve the two open
questions (a second `s_rslGlobals` table's relationship to the primary
seeding path; `Ol` wiring consistency) as part of the same effort, since
both live in the exact code region items 1-2 touch; (5) add a differential
regression-guard test (`.slo`-embedded `usedParameters` vs. the
`.rslo`-computed equivalent for the same source) so future drift between
the two backends' computation is caught automatically rather than requiring
manual re-transcription — mirroring the dynamic coverage-guard precedent
from spec `011-jit-opcode-parity`.

## Technical Context

**Language/Version**: C++20 (constitution Principle II) for all new/modified code; no new language surface — the fix is a scan/table change inside existing compiler and shading-runtime translation units.

**Primary Dependencies**: LLVM (IR emission — `llvmEmitter.cpp` already depends on it), CMake. No new external dependency introduced. The X-macro re-expansion approach requires `libshader_compiler` to see `libshader_shading`'s opcode/function table headers at compile time (header-only; no new link dependency).

**Storage**: N/A — `.rslo`/`.slo` are compiled shader files on disk; no schema/storage layer touched.

**Testing**: `ctest`, across three tiers (see research.md D6 for the full
rationale — table-parity alone would not have caught the original bug,
since D1 confirms the old `kParamBits` table's *values* were always
correct):
1. Table-parity guard + gating-condition unit tests — `-L libshader`
   (existing label; `libshader_compiler`-only link, same linkage as today's
   `test_libshader_compiler`/`test_libshader_opcode_coverage`).
2. Live differential oracle (FR-012's literal requirement — compares
   `CShader::usedParameters` loaded via the real `CRenderer::context->getShader()`
   path for both a `.slo` and `.rslo` compile of the same source) — a new
   `tests/shading_parity/` directory, linking `openrender_common_flags` +
   `ri` + `libshader_shading` + `openrendercommon`, following the exact
   precedent already established by `tests/imager/` (`test_imager_execution`
   et al.) for tests that need a live `CRenderer::contexts` shading context.
   This cannot be a `libshader_compiler`-only unit test: the interpreter's
   `usedParameters` ground truth is computed by `shading/rslo.y` at
   `.rslo`-load time from live `CRenderer` declaration state, not a
   self-contained computation the way the JIT's compile-time-embedded
   metadata is. The `Ol` wiring test (tasks.md T024) lives here too, for the
   same reason — it needs a real loaded shader instance on both backends,
   not just a compiled bitmask. `s_rslGlobals` (FR-009) has no dedicated
   automated test asset in either tier; it is resolved via a written
   determination recorded in research.md (tasks.md T025), not a test.
3. `-L visual` (8×8 block-average image-diff regression, thresholds
   20-40/255 — exercises the fixed default-fill/derivative/raytrace/
   message-passing/ambient behavior end-to-end via real `.slo` bitcode after
   `cmake --install` regenerates it). This is also SC-001's durable
   automated guard once tasks.md's new fixture task (T032) is
   implemented — the differential oracle (tier 2) only checks the
   `usedParameters` bitmask, not rendered pixels.

**Target Platform**: Linux and macOS (constitution Principle VI). No new JIT-callable `op_*`/`rsl_*` symbols are introduced by this feature (the fix is confined to compile-time bitmask computation and two small interpreter-side runtime edits to existing functions), so the macOS JIT dead-stripping gotcha (CLAUDE.md #3) does not newly apply.

**Project Type**: Not a standalone app/service — a correctness-parity fix confined to two existing libraries within the single `orender` C++ CLI renderer: the compiler frontend (`libshader_compiler` — `llvmEmitter.cpp`, and its `CMakeLists.txt` for the new include path) and the runtime shading engine (`libshader_shading` — `execute.cpp`, `shading.cpp`, read-only reference use of `rslo_code.h`'s X-macro pattern and the opcode/function table headers). No new executable, service, or public interface.

**Performance Goals**: None gated by this feature — see spec Clarifications (2026-08-27): the derivative-footprint bit fix (Story 2) is judged solely on whether the bit now matches the interpreter's; no measurable speedup threshold is required, unlike spec 011's FR-011 timing gate.

**Constraints**:
- Delegation/no-drift (supports FR-001 via research.md D3): the JIT compiler's `usedParameters` computation must be derived from the interpreter's own opcode/global tables (X-macro re-expansion), not a second hand-maintained copy — this is the direct fix for the bug class this feature exists to close (a hand-transcribed table drifted from the reference table it was meant to mirror).
- FR-011 (reference implementation): the interpreter (`.rslo`) side of `usedParameters` computation must not change; only the two narrowly-scoped ambient-`Cl` fixes (FR-007, FR-008) touch interpreter behavior at all.
- Architectural: `libshader_compiler` and `libshader_shading` currently have zero link dependency in either direction (confirmed both `CMakeLists.txt`s). The X-macro approach must stay header-only (a new `PRIVATE` include path) to preserve this — it must not introduce a link dependency from the compiler into the shading runtime library.
- Fallback (explicit, from the originating plan): if the X-macro re-expansion hits a real macro-arity mismatch during implementation, hand-transcribe the same table with a comment citing exact source lines, and rely on the Phase-4-equivalent coverage-guard test to catch future drift instead. Try the X-macro route first.

**Scale/Scope**: Bounded by the four validated-finding groups in spec.md's User Stories 1-4 (Ci/Oi default-fill, the remaining `usedParameters` bit gaps, ambient-`Cl` crash/double-count, and the two open-question cleanups). Single feature branch, no multi-repo/multi-service scope. Explicitly out of scope: the unrelated, already-tracked intermittent SIGSEGV on `subdiv-loop-photon.rib` (different subsystem, no evidence of shared root cause).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Gate | Status |
|---|---|---|
| I. Clean Code | Small, focused functions; no magic numbers/deep nesting | **PASS** — the fix collapses two mechanisms (a wrong `SLC_GLOBAL`-gated scan plus a separate hand-written 3-opcode `hasNonAmbientOp` check) into one table-driven scan, reducing special-case branching rather than adding it. |
| II. Language Standards | C++20/C17, no non-standard extensions | **PASS** — X-macro re-expansion is the same pattern already in production use (`rslo_code.h`); no new language surface. |
| III. Test-Driven Development (NON-NEGOTIABLE) | Red→Green→Refactor; tests before implementation | **PASS, with a sequencing requirement carried into `/speckit-tasks`**: the differential `usedParameters` test (FR-012) and the crash reproduction for User Story 3 must be written and shown to fail *before* the corresponding compiler/runtime fixes land — tasks.md must not reorder this. |
| IV. Command Line Interface | Functionality via CLI, stdin/stdout/stderr | **PASS (no new surface)** — no new CLI added; existing `oshader --jit`/`orender` behavior is corrected, not extended. |
| V. Minimal Dependencies | No new dependency without justification | **PASS** — zero new external dependencies; the only new build-graph edge is a header-only include path within the existing two libraries. |
| VI. Platform Targeting | Linux/macOS only | **PASS** — no platform-specific code added; no new JIT-callable symbols to subject to the dead-stripping gotcha. |
| VII. Documentation and Site Management | Docs kept in sync | **PASS** — this is an internal correctness fix with no user-visible behavior change beyond "shaders now render correctly," so no new Hugo `site/` page is required; `DEVNOTES.md`'s spec-branch list gets a one-line addition per this repo's existing convention. |

No violations requiring Complexity Tracking justification — the fix reduces
existing complexity (one drifted hand-maintained table collapsed into a
single delegated-from-source-of-truth mechanism) rather than adding it.

## Project Structure

### Documentation (this feature)

```text
specs/014-jit-shading-parity/
├── plan.md              # This file (/speckit-plan command output)
├── research.md          # Phase 0 output (/speckit-plan command)
├── data-model.md         # Phase 1 output (/speckit-plan command)
├── quickstart.md         # Phase 1 output (/speckit-plan command)
├── contracts/            # Phase 1 output (/speckit-plan command)
└── tasks.md              # Phase 2 output (/speckit-tasks command - NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
src/
├── libshader/
│   ├── compiler/                    # libshader_compiler — .sl → .rslo/.slo
│   │   ├── llvmEmitter.cpp            # usedParameters computation: replaced with a single IR-instruction
│   │   │                              #   scan against a combined global-variable + opcode/function table;
│   │   │                              #   hand-written hasNonAmbientOp block deleted (superseded);
│   │   │                              #   s_rslGlobals table investigated/collapsed if redundant (FR-009)
│   │   ├── llvmEmitter.h              # export point for the new computeUsedParameters-equivalent, next
│   │   │                              #   to the existing kHandledOpcodes testability precedent (spec 011)
│   │   ├── rslo.cpp                   # CScriptContext global-seeding constructor — root-cause context,
│   │   │                              #   NOT modified (the fix changes the downstream filter, not the
│   │   │                              #   seeding, since other consumers may depend on the full seed list)
│   │   ├── irBuilder.cpp              # buildVarTable() — root-cause context, not modified
│   │   └── CMakeLists.txt             # NEW: PRIVATE include path onto src/libshader/shading (header-only,
│   │                                   #   for the X-macro re-expansion; no new link dependency)
│   │
│   ├── runtime/                     # libshader_runtime — .rslo/.slo loader (not touched by this feature)
│   │
│   └── shading/                     # libshader_shading — interpreter + JIT runtime support
│       ├── rslo_code.h                # X-macro precedent this feature's compiler-side table mirrors
│       │                              #   (read-only reference; not modified)
│       ├── shaderOpcodes.h /           # interpreter opcode/function → PARAMETER_* tables — the source of
│       │   shaderFunctions.h /         #   truth the new compiler-side scan re-expands via X-macro; not
│       │   giOpcodes.h / giFunctions.h #   modified (this feature reads them, does not change their content)
│       ├── execute.cpp                # interpreter execEnd: block gets the missing *alights != nullptr
│       │                              #   guard (FR-007), copied verbatim from the existing JIT-mirror
│       │                              #   guard already present in this same file
│       ├── shading.cpp                # complete() (both overloads) — default-fill fallback now reachable
│       │                              #   for JIT once FR-001 lands (no code change needed here beyond
│       │                              #   what already exists, verified by SC-001); prepareAmbient()'s
│       │                              #   duplicate re-accumulation loop removed (FR-008), mirroring the
│       │                              #   already-correct callAmbient() sibling; Ol consumers audited
│       │                              #   for consistency with Cl (FR-010)
│       └── RSLShading.cpp             # shade() — the direct-execute() entry point that makes the FR-007
│                                       #   null-guard fix reachable/necessary; not modified itself
│
├── ri/
│   └── rendererDeclarations.cpp       # declareVariable(name, ..., PARAMETER_*) — authoritative
│                                       #   interpreter-side name→bit table, reference only, not modified
│
tests/
├── libshader/                       # ctest -L libshader — table-parity + gating-condition unit tests
│                                     #   (compiler-only link, no ri/libshader_shading) and the
│                                     #   s_rslGlobals/Ol determination tests land here
│
└── shading_parity/                  # NEW — live differential oracle for FR-012, mirroring tests/imager/'s
                                       #   linkage (openrender_common_flags, ri, libshader_shading,
                                       #   openrendercommon) since the interpreter's usedParameters ground
                                       #   truth requires a real CRenderer::contexts shading context to
                                       #   compute (shading/rslo.y at .rslo-load time), not a compiler-only
                                       #   computation
```

**Structure Decision**: Single-project C++ library layout — this is a
compiler/runtime-backend parity fix inside one C++ CLI renderer, so none of
the template's generic multi-project options apply. The tree above keeps
the existing `libshader_compiler` ⇄ `libshader_shading` module boundary
intact (zero link dependency in either direction, preserved as a hard
constraint above) and adds exactly one new build-graph edge: a header-only
`PRIVATE` include path so the compiler can re-expand the shading runtime's
own opcode/function tables at compile time instead of hand-duplicating
them.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No entries — no violations were identified.

## Post-Phase-1 Constitution Re-Check

Re-evaluated after `research.md`/`data-model.md`/`contracts/`/`quickstart.md`
were generated: all seven gates from the initial Constitution Check above
still **PASS** unchanged. Phase 0 surfaced one design consequence not
visible at Technical-Context time — the FR-012 differential oracle
(research.md D6/D7) needs a new `tests/shading_parity/` directory linking
`ri` + `libshader_shading` in full, since the interpreter's `usedParameters`
ground truth is computed at `.rslo`-load time from live `CRenderer`
declaration state, not a self-contained computation. This is not a new
architectural pattern (`tests/imager/` already links the identical set for
the identical reason — a live `CRenderer::contexts` shading context), so it
does not change Principle V's "no new dependency without justification"
assessment: zero new external dependencies, and the new internal
test-to-`ri` link edge mirrors an existing, working precedent rather than
introducing a novel one. No Complexity Tracking entry is warranted.
