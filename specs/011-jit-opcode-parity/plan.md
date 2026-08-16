# Implementation Plan: LLVM JIT Opcode-Coverage Parity Sweep

**Branch**: `011-jit-opcode-parity` | **Date**: 2026-08-16 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/011-jit-opcode-parity/spec.md`

**Note**: This template is filled in by the `/speckit-plan` command; its definition describes the execution workflow.

## Summary

The `.slo` (LLVM JIT) shading backend silently drops or misroutes several
RSL opcodes that the `.rslo` interpreter handles correctly — confirmed for
`cfrom`/`mfrom` (silent drop, no emitted IR, no diagnostic) and `ctransform`
(silently misrouted into the unrelated `pfrom` point-transform family,
producing wrong pixels, not a drop). `llvmEmitter.cpp`'s `emitFunction()`
dispatch is an `if/else-if` chain with no final `else`, so any opcode it
doesn't recognize vanishes silently — `llvm::verifyModule` only checks
structural validity of what *was* emitted and is blind to this bug class.

The technical approach: (1) triage the full `opcodes.cpp` vs.
`llvmEmitter.cpp` mnemonic diff (~48 raw candidates, inflated by
string-padding and a `gatherHeader`/`gatherhdr` case mismatch) down to a
confirmed-reachable list; (2) fix every confirmed gap by adding a JIT-callable
`op_*` wrapper (`src/libshader/shading/rslOps.cpp`, following the existing
`op_pfrom` template) that delegates to the *same* function the interpreter
already calls — never new shading math in the JIT path; (3) as part of this,
relocate `convertColorFrom`/`convertColorTo` (`src/ri/init.cpp:67,228`) into
`src/common/` (`openrendercommon`, already embedded in both `libri` and
`libshader_shading` build outputs) so both the interpreter's existing
`cfrom`/`ctransform` call sites and the new JIT wrappers reach the same
function *without* a reverse link dependency from shader-libs into `ri` —
mirroring the established `Ri*StepFilter` relocation precedent
(`src/common/rslConstants.cpp`, see `DEVNOTES_DETAILS/BUGS.md` Resolved
section) rather than extending the one remaining reverse `extern` reference
(`execute.cpp:53`) that motivated this decision; (4) build a dynamic
opcode-coverage regression guard, backed by a single shared
`static const char* const[]` table both `emitFunction()` and a new
`libshader`-labeled ctest consult, so the guard re-derives the reachable set
at test-run time and can never be checked at render runtime; (5) converge
`jitArea`/`jitCalculateNormal`/`jitDepth`/`jitDuVector`/`jitDvVector`
(`shading.cpp`) from hand-duplicated interpreter logic onto the same
delegation pattern `op_specular_batch` already uses; (6) fix the
`DEVNOTES_DETAILS/BUGS.md`, `OSHADER_UPDATES.md`, and `CLAUDE.md` doc/reality
drift discovered during investigation.

## Technical Context

<!--
  ACTION REQUIRED: Replace the content in this section with the technical details
  for the project. The structure here is presented in advisory capacity to guide
  the iteration process.
-->

**Language/Version**: C++20 (constitution Principle II) for all new/modified code; `op_*`/`rsl_*` JIT-callable wrappers keep C linkage (existing convention, `src/libshader/shading/rslOps.h`).

**Primary Dependencies**: LLVM (ORC JIT, IR emission — `core orcjit native bitreader support` per `src/libshader/shading/CMakeLists.txt`), CMake. No new external dependency introduced.

**Storage**: N/A — `.rslo`/`.slo` are compiled shader files on disk, not a database; no schema/storage layer touched.

**Testing**: `ctest`, using this project's existing label conventions: `-L libshader` (compiler/runtime unit tests — the new dynamic coverage-guard test lands here) and `-L visual` (8×8 block-average image-diff regression, thresholds 20-40/255 — new `-slo` cases added per fixed opcode category). A new manual-only label (`-L perf-manual`, exact name finalized in tasks) holds the FR-011 JIT-vs-interpreter timing-ratio test; it MUST NOT be included in the project's default/CI `ctest` invocations (per user direction: build the assertion, but only run it manually under controlled machine conditions).

**Target Platform**: Linux and macOS (constitution Principle VI). New `op_*` symbols are subject to the macOS JIT dead-stripping gotcha (CLAUDE.md #3) — each must be verified to resolve at JIT bind time via the existing `DynamicLibrarySearchGenerator`; only add `jitSymbolRetain.cpp`-style retention machinery if one doesn't (see FR-010 doc-drift note — that file is currently referenced by docs but does not exist).

**Project Type**: Not a standalone app/service — a correctness-parity fix spanning three existing libraries within the single `orender` C++ CLI renderer: the compiler frontend (`libshader_compiler` — `llvmEmitter.cpp`), the runtime shading engine (`libshader_shading` — `rslOps.cpp`, `shading.cpp`), and (new in this plan) a small relocation into the shared utility library (`openrendercommon`). No new executable, service, or public interface is introduced.

**Performance Goals**: FR-011 — for every construct fixed under FR-001/002/003/005, JIT rendering time for a demonstrating shader MUST be ≤ 90% of the interpreter's rendering time for the same shader. Verified via a documented manual benchmark procedure (quickstart.md) plus a `ctest`-registered assertion that is never run as part of the default suite (see Testing above) — not treated as a CI gate, to avoid flaky failures from machine-load variance.

**Constraints**:
- FR-007 (delegation-only): every JIT fix must call the same final function/method the `.rslo` interpreter already calls internally — never reimplement shading math independently in the JIT path.
- FR-009: the interpreter's (`.rslo`) own behavior and output must not change.
- FR-006: the dynamic opcode-coverage guard is a test-suite-time-only check (never at render runtime) and must re-derive the reachable-opcode set at test-run time, not from a list frozen at the end of this feature.
- Architectural (reinforced by user, elevated to primary scope by this plan): the separation of concerns between `ri` (surface/geometry) and the shader-libs (`libshader_compiler`/`libshader_runtime`/`libshader_shading` — compilation/load/execution) MUST be obeyed and enforced, not merely preserved. Investigation found this boundary is *already* crossed today — `execute.cpp:53` holds an unresolved `extern` into `src/ri/init.cpp`'s `convertColorFrom`/`convertColorTo`, used by the interpreter's own `cfrom`/`ctransform` macros — contradicting `libshader_shading/CMakeLists.txt`'s documented one-way dependency (`ri` depends on it, not vice versa). Rather than extending this reverse reference to the new JIT wrappers, this plan relocates `convertColorFrom`/`convertColorTo` into `src/common/` (`openrendercommon`, already linked into both `libri` and `libshader_shading`), which both the interpreter's existing call sites and the new `op_cfrom`/`op_ctransform` wrappers then call directly — eliminating the reverse dependency for both backends at once. This mirrors the already-established `Ri*StepFilter` relocation (`src/ri/ri.cpp` → `src/common/rslConstants.cpp`, `DEVNOTES_DETAILS/BUGS.md` Resolved section), so it is a proven pattern in this codebase, not a novel one. `ECoordinateSystem` itself stays defined in `src/ri/rendererc.h` (a header-only, compile-time-only cross-reference `libshader/shading/shader.h` already makes today) — only the function *definitions* (link-time symbols) move.

**Scale/Scope**: Bounded by FR-004's reachability inventory — Phase 0 triage of ~48 raw candidate opcode-gap strings (matrix arithmetic, `gather()`/`gatherElse`/`gatherEnd`, comparison/logic, array move ops), expected to shrink once string-padding and case-mismatch artifacts are excluded. Single feature branch, no multi-repo or multi-service scope. The `convertColorFrom`/`convertColorTo` relocation touches exactly two existing call-site files (`execute.cpp`, and the `shaderOpcodes.h`/`shaderFunctions.h` macros) plus the two functions' new home — not a wider refactor of `src/ri/init.cpp`.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Gate | Status |
|---|---|---|
| I. Clean Code | Small, focused functions; no magic numbers/deep nesting | **PASS** — new `op_*` wrappers clone the existing `op_pfrom` shape (one focused responsibility each); the `convertColorFrom`/`convertColorTo` relocation *removes* an architectural wart (reverse dependency) rather than adding one. |
| II. Language Standards | C++20/C17, no non-standard extensions | **PASS** — matches existing `rslOps.h`/`.cpp` C-linkage convention; no new language surface. |
| III. Test-Driven Development (NON-NEGOTIABLE) | Red→Green→Refactor; tests before implementation | **PASS, with a sequencing requirement carried into `/speckit-tasks`**: the Phase 0 triage repros (image divergence before fix) and the coverage-guard test must be written and shown to fail *before* the corresponding `op_*`/`llvmEmitter.cpp` fixes land — tasks.md must not reorder this. |
| IV. Command Line Interface | Functionality via CLI, stdin/stdout/stderr | **PASS (no new surface)** — no new CLI added; existing `oshader --jit`/`orender` behavior unchanged except for the opcodes this feature fixes. |
| V. Minimal Dependencies | No new dependency without justification | **PASS** — zero new external dependencies; LLVM was already required. |
| VI. Platform Targeting | Linux/macOS only | **PASS** — no platform-specific code added beyond the existing JIT dead-stripping mitigation pattern (CLAUDE.md #3), applied per new symbol as needed. |
| VII. Documentation and Site Management | Docs kept in sync | **PASS** — FR-010 covers `DEVNOTES_DETAILS/BUGS.md`, `OSHADER_UPDATES.md`, `CLAUDE.md` corrections; this is an internal correctness fix with no user-visible behavior change, so no new Hugo `site/` page is required. |

No violations requiring Complexity Tracking justification — the one structural change (relocating `convertColorFrom`/`convertColorTo`) reduces existing complexity/coupling rather than adding it.

## Project Structure

### Documentation (this feature)

```text
specs/[###-feature]/
├── plan.md              # This file (/speckit-plan command output)
├── research.md          # Phase 0 output (/speckit-plan command)
├── data-model.md        # Phase 1 output (/speckit-plan command)
├── quickstart.md        # Phase 1 output (/speckit-plan command)
├── contracts/           # Phase 1 output (/speckit-plan command)
└── tasks.md             # Phase 2 output (/speckit-tasks command - NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
src/
├── common/                        # openrendercommon — shared, embedded into BOTH libri and libshader_shading
│   ├── colorSpace.h                 # NEW: convertColorFrom/convertColorTo declarations (relocated)
│   ├── colorSpace.cpp                # NEW: relocated function bodies (verbatim math, no behavior change)
│   └── rslConstants.cpp             # precedent for this relocation pattern (Ri*StepFilter, see BUGS.md)
│
├── ri/                             # Surface/geometry layer — RenderMan interface + renderer core
│   └── init.cpp                     # convertColorFrom/convertColorTo REMOVED from here (moved to common/)
│
├── libshader/
│   ├── compiler/                    # libshader_compiler — .sl → .rslo/.slo
│   │   ├── llvmEmitter.cpp            # emitFunction() dispatch: new cfrom/mfrom/ctransform/... cases;
│   │   │                              #   refactored to consult a shared kHandledOpcodes[] table (FR-006)
│   │   ├── opcodes.cpp / opcodes.h    # canonical opcode mnemonics — source of truth for the Phase 0
│   │   │                              #   reachability inventory and the coverage-guard table
│   │   └── irBuilder.cpp              # gatherHeader/gatherhdr case mismatch resolved during triage
│   │
│   ├── runtime/                     # libshader_runtime — .rslo/.slo loader (not touched by this feature)
│   │
│   └── shading/                     # libshader_shading — interpreter + JIT runtime support (one-way:
│       │                             #   does NOT link ri; ri links this)
│       ├── rslOps.h / rslOps.cpp      # NEW: op_cfrom/op_mfrom/op_ctransform (+ any Phase 3 wrappers),
│       │                              #   cloned from the existing op_pfrom template; call src/common/
│       │                              #   colorSpace.h directly — no reverse reference into ri
│       ├── shaderOpcodes.h            # interpreter macros (CFROMEXPR, MFROMEXPR, ...) — updated to call
│       │                              #   the relocated src/common function, same behavior (FR-009)
│       ├── shaderFunctions.h          # interpreter macros (CTRANSFORMEXPR, ...) — same update
│       ├── execute.cpp                # existing `extern convertColorFrom/convertColorTo` (execute.cpp:53)
│       │                              #   removed; replaced with `#include "colorSpace.h"`
│       └── shading.cpp                # jitFindCoordinateSystem (reused, unchanged); jitArea/
│                                       #   jitCalculateNormal/jitDepth/jitDuVector/jitDvVector converge
│                                       #   onto delegation (Phase 4)
│
tests/
├── libshader/                       # ctest -L libshader — new coverage-guard test lands here (FR-006)
└── visual/                          # ctest -L visual — new -slo add_visual_test() cases per fixed
                                      #   category (CMakeLists.txt ~line 247-287 pattern); a new
                                      #   perf-manual-labeled timing test (FR-011) is registered but
                                      #   excluded from default/CI ctest invocations
```

**Structure Decision**: Single-project C++ library layout — none of the
template's generic Options (web app, mobile+API) apply to a compiler/runtime
library feature inside one C++ CLI renderer, so the tree above is this
feature's concrete structure decision. It deliberately keeps the existing
`ri` ⇄ `libshader_*` module boundaries intact and, per the reinforced
separation-of-concerns constraint, actively repairs the one place that
boundary was already crossed (`convertColorFrom`/`convertColorTo`) by moving
those two functions into `src/common/` — the library both sides already
depend on — rather than adding a second reverse reference for the JIT path.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No entries — no violations were identified before or after Phase 1 design.

## Post-Phase-1 Constitution Re-Check

Re-evaluated after `research.md`/`data-model.md`/`contracts/`/`quickstart.md`
were generated: all seven gates from the initial Constitution Check above
still **PASS** unchanged. The one structural design decision made during
Phase 0 (D2: relocating `convertColorFrom`/`convertColorTo` into
`src/common/`) strengthens Principle I (Clean Code — removes an existing
reverse-dependency wart) rather than introducing new complexity, so no
Complexity Tracking entry is warranted.
