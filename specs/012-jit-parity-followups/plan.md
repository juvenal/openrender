# Implementation Plan: JIT/Interpreter Parity Follow-ups (post-011)

**Branch**: `012-jit-parity-followups` | **Date**: 2026-08-24 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/012-jit-parity-followups/spec.md`

**Worktree**: `/Volumes/Projects/Development/CLI/openrender-worktrees/012-jit-parity-followups`
(the main checkout stays on `master`; all work for this feature happens here)

## Summary

Three defects/gaps deferred out of `specs/011-jit-opcode-parity`, delivered as
three independently verifiable streams over one shared verification baseline:

1. **US1 (P1) — `usfroma` crash.** A varying-index read of a `uniform string`
   array crashes the `.rslo` interpreter. Fixed only after an empirical
   reproduction (FR-002) establishes whether the root cause is in the
   interpreter or in the bytecode the compiler emits; either edit is gated by
   a mandatory maintainer STOP. Permanent executing coverage arrives as a new
   shader + new scene + new reference image, never by editing an existing one.
2. **US2 (P2) — uniform-dispatch cost.** The JIT emits every instruction as a
   full `numVerts`-wide `op_*` call; the interpreter runs uniform-classified
   instructions once. Phase 0 settled the spec's one open question: the
   classification **is** available at the emitter's dispatch-construction
   point, derivable from the strides the emitter already holds
   ([research.md D1](./research.md)). The change collapses a uniform
   instruction's call to `n = 1` **with a null tag pointer**
   ([D2](./research.md), [contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md)),
   at every family where the interpreter short-circuits, with `DEFLIGHTFUNC`
   as the one explicitly-listed exclusion.
3. **US3 (P3) — light-iteration duplication.** The interpreter's
   `runLightsTemplate` macro and the JIT's `CShadingContext::runCategoryLights`
   method are hand-synced copies that have already drifted in two places
   ([D6](./research.md)). They converge onto one implementation reproducing
   the macro's semantics exactly, keeping the interpreter bit-unchanged.

The technical approach is delegation throughout (FR-010): no shading math is
written in this feature. US2 changes *how wide* an existing `op_*` call is;
US3 changes *which single function* both backends reach.

## Technical Context

**Language/Version**: C++20 (per Constitution Principle II); the affected
compiler frontend is Bison/Flex-generated C++ (`rslo.y`)

**Primary Dependencies**: LLVM ORC JIT (`LLJIT`) for the `.slo` backend; CMake
≥ project minimum; no new dependency is introduced by this feature

**Storage**: N/A — compiled shader artifacts (`.rslo` bytecode, `.slo` LLVM
bitcode) are build outputs, not persistent state

**Testing**: CTest. `ctest --test-dir build -L libshader` (compiler/unit,
including the `LibShader_OpcodeCoverage` guard at
`src/libshader/tests/test_opcode_coverage.cpp`); `ctest --test-dir build -L visual`
(87+ scene regression, 8×8 block-averaged diff, thresholds 20–40/255);
`ctest --test-dir build -L perf-manual` (six JIT-vs-interpreter timing scenes,
never run by default or in CI)

**Target Platform**: macOS and Linux (Constitution Principle VI). Development
and measurement for this feature happen on macOS (darwin 25.5.0)

**Project Type**: Renderer engine — a C++ library set plus CLI executables. No
frontend/backend split; the single-project layout applies

**Performance Goals**: JIT-to-interpreter wall-clock ratio improves by more
than each scene's measured run-to-run variance on every measurement scene with
meaningful uniform computation, with zero regressions (SC-004); the ratio gap
between `sphere-arrayops` (uniform-dominated) and `sphere-cfrom` (near-zero
uniform density) narrows beyond variance (SC-006). Spec 011's 90% stretch bar
is reported per scene, not gated (SC-005)

**Constraints**:
- Zero rendered-output difference above the same-configuration noise floor
  across all three changes; **no pre-existing reference image is regenerated**
  (SC-007)
- The `.rslo` interpreter is the reference implementation; it changes only for
  a confirmed defect with an empirical reproduction, under a mandatory
  maintainer STOP, using the narrowest possible change (FR-011). The same STOP
  applies to compiler-side fixes, whose presentation must additionally state
  that a compiler change alters the meaning of already-compiled artifacts for
  **both** backends
- Behaviour-preserving refactors are exempt from the STOP but not from
  before/after verification; the exemption lapses the moment observable
  behaviour would change
- Delegation only (FR-010): every fix calls the same final function the
  interpreter already calls
- Layering: `ri` depends on `libshader_shading`, never the reverse
- No automatic commits at any point
- `.slo` bitcode staleness: nothing in the build graph regenerates `.slo` in
  either the tracked `shaders/` tree or the deploy tree; an ABI mismatch
  between stale bitcode and current runtime C++ is caught at neither build nor
  link time. Every `-slo` verification is preceded by `stat`-ing each `.slo`
  against both the emitter source and the `oshader` binary

**Scale/Scope**: 3 user stories; ~4 source files expected to change
(`llvmEmitter.cpp`, `shading.cpp`, `execute.cpp` and/or one compiler file for
US1); 1 new shader + 1 new scene pair + 1 new reference image; 2 documentation
files under FR-012. No new public API, no new RSL language surface, no new
scene or shader validity rules

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

Evaluated against `.specify/memory/constitution.md` v1.1.0.

| Principle | Status | Basis |
|---|---|---|
| I. Clean Code | **PASS** | US3 *removes* a duplicated implementation (SC-008). US2 adds one predicate at an existing dispatch point, no new subsystem |
| II. Language Standards (C++20/C17, strict compliance, C-linkage conventions) | **PASS** | No language-level change. The `op_*` C-linkage ABI is explicitly **not** modified — the uniform collapse reuses the existing `(n, tags)` parameters ([D2](./research.md)) |
| III. Test-Driven Development (**NON-NEGOTIABLE**) | **PASS with per-story mapping** — see below | |
| IV. CLI Interface | **PASS** | No CLI surface changes. `oshader`/`orender` flags and behaviour are untouched |
| V. Minimal Dependencies | **PASS** | Zero new dependencies |
| VI. Platform Targeting (Linux/macOS only) | **PASS** | No platform-specific code. macOS JIT symbol resolution is unchanged — no new `op_*` symbol is introduced by US2 or US3 |
| VII. Documentation and Site Management (Hugo `site/`) | **PASS** | FR-012 updates the Hugo site at `docs/site/content/development/releases.md` (T052) alongside the internal notes `DEVNOTES_DETAILS/BUGS.md`, `DEVNOTES_DETAILS/OSHADER_UPDATES.md`, and spec 011's records. The site entry is proportionate: the US1 crash is reachable from ordinary RSL source and the US2 work changes user-observable JIT performance. No exemption is claimed. The existing `.github/workflows` deployment automation is unchanged by this feature and requires no task |

### Principle III mapping (Red before Green, per story)

Principle III is flagged NON-NEGOTIABLE, so each story names its failing-test
artifact explicitly rather than asserting compliance:

- **US1 — Red is FR-002's empirical reproduction.** A minimal shader + scene
  that terminates the interpreter abnormally in ≥5 of 5 runs (SC-001). This
  reproduction is simultaneously the Red test and the authorization for the
  fix; no source may be edited before it exists and has been presented at the
  STOP. Green is the same scene rendering normally and its new reference image
  matching, registered as an executing regression test (SC-002).
- **US2 — Red is the measurement baseline, and it has two halves.** (a) The
  run-to-run variance baseline, which **does not exist yet** — spec 011
  reported single-run ratios only ([D7](./research.md)) — and which both
  SC-004 and SC-006 are defined relative to; establishing it is a first-class
  task, not setup. (b) The pre-change per-scene ratios on the same quiescent
  machine. Green is the post-change ratios beating variance on every
  uniform-dense scene with zero regressions, plus zero image differences above
  the noise floor.
- **US3 — Red is the before/after render set.** A refactor's test is that
  nothing moves: the full `-slo` and `-rslo` visual suite captured against the
  unchanged binary, then re-run against the converged one. FR-009's refactor
  exemption removes the STOP, not the verification.

**Result: PASS.** No violations to justify; Complexity Tracking stays empty.

## Project Structure

### Documentation (this feature)

```text
specs/012-jit-parity-followups/
├── plan.md                        # This file (/speckit-plan output)
├── spec.md                        # Feature specification
├── research.md                    # Phase 0 output — D1..D9, resolves the spec's open Assumption
├── data-model.md                  # Phase 1 output — entities, states, invariants
├── quickstart.md                  # Phase 1 output — runnable validation guide
├── contracts/                     # Phase 1 output
│   ├── op-uniform-collapse.md     #   op_* C-linkage collapse contract (n=1, tags=nullptr)
│   └── light-iteration.md         #   single shared light-iteration entry point (SC-008)
├── checklists/
│   └── requirements.md            # Spec quality checklist (16/16, from /speckit-specify)
├── measurements.md                # Implementation output — the single evidence ledger:
│                                  #   density classification, variance, before/after ratios,
│                                  #   SC-001..SC-008 disposition (created T004)
├── baselines/                     # Implementation output — captured run logs, version-tracked
│                                  #   so a "before" survives the session and is auditable.
│                                  #   NOT /tmp: a /tmp baseline cannot be diffed after a reboot
│                                  #   and cannot be reviewed alongside the change it justifies
└── tasks.md                       # Phase 2 output (/speckit-tasks — NOT created here)
```

### Source Code (repository root)

```text
src/libshader/
├── compiler/                      # RSL → .rslo / .slo
│   ├── llvmEmitter.cpp            # US2 PRIMARY: uniform predicate + collapsed dispatch
│   │                              #   getVar() strides (442), dstDesc.stride (598),
│   │                              #   emitBin/emitUn/emitTern (459-509), 54 numVerts sites
│   ├── llvmEmitter.h              # kHandledOpcodes (read by the coverage guard)
│   ├── ir.h                       # IRInstr (90-97) — no uniform flag; strides used instead
│   ├── rslo.y                     # US1 CANDIDATE root cause (compiler side); opcodeUniform origin
│   └── opcodes.cpp                # canonical mnemonics
├── shading/                       # execution engine (both backends)
│   ├── execute.cpp                # US1 CANDIDATE root cause (interpreter side);
│   │                              #   DEFOPCODE/DEFFUNC/DEFSHORT* short-circuit (610-718);
│   │                              #   US3 runLightsTemplate macro (422-517)
│   ├── shading.cpp                # US3 PRIMARY: runLights/runCategoryLights (1502-1558),
│   │                              #   jitIlluminanceBegin/Next (2059-2122)
│   ├── rslOps.cpp                 # op_* C-linkage wrappers; ACTIVE()/IDX() (40-43)
│   ├── shaderFunctions.h          # DEFFUNC / DEFSHORTFUNC builtins
│   └── shaderOpcodes.h            # illuminance arities (90/92/144/146)
└── tests/
    ├── test_opcode_coverage.cpp   # reachability-only guard (does NOT execute usfroma)
    └── CMakeLists.txt             # label "libshader;compiler;unit"

shaders/                           # US1: new probe shader lands here (+ regenerated .slo/.rslo)
examples/rib/tests/                # US1: new -reyes / -reyes-slo scene pair
tests/visual/
├── CMakeLists.txt                 # add_visual_test (US1 new scene); add_perf_manual_test (236),
│                                  #   six measurement scenes (1135-1157)
└── reference/                     # US1: ONE new reference image; no existing image regenerated

DEVNOTES_DETAILS/
├── BUGS.md                        # FR-012
└── OSHADER_UPDATES.md             # FR-012
```

**Structure Decision**: Single-project engine layout, unchanged. This feature
adds no directory and no build target. All three streams land inside
`src/libshader/` — US2 in `compiler/`, US3 in `shading/`, US1 in whichever of
the two the reproduction implicates — plus test/scene assets under
`shaders/`, `examples/rib/tests/`, and `tests/visual/`. The `ri` →
`libshader_shading` dependency direction is preserved; nothing in this feature
introduces a reverse edge.

## Execution Model (maximum parallelism)

The spec's Assumptions state the three issues are independent: *"none blocks
another, and each is separately deliverable and separately verifiable."* The
plan exploits that fully, but three environment-level constraints
([D8](./research.md)) serialize specific steps no matter how tasks are
ordered. Both facts are encoded here so `/speckit-tasks` can mark `[P]`
honestly rather than optimistically.

### Serial prerequisite — Stream 0 (blocks all three streams)

| Step | Why it cannot be parallelised |
|---|---|
| S0.1 Build the unchanged binary, then **generate** the full artifact set and **provision** the deploy tree — this worktree is fresh: `shaders/` holds only `.sl` sources (verified 2026-08-25: zero `.slo`, zero `.rslo`) and `openrender/` does not exist at all, so there is nothing yet to audit for staleness. Compile every `.rslo` and `.slo` (`SHADERS_INCLUDE` for header-including shaders, never `-I`), run `cmake --install build --prefix "$(pwd)/openrender"`, copy the artifacts in, and smoke-test one render. Establish a working LLVM-IR dump path in the same step — `oshader` has no IR-dump flag and `llvm-dis` is not installed anywhere reachable, so US2's emitted-form check has no tool until one is provided. The `stat` staleness audit becomes meaningful only from the first regeneration onward (US2/US3), where a stale `.slo` produces garbage at JIT call sites, silently | Everything downstream reads these artifacts; generating them mid-stream would make one stream's baseline describe a different tree than another's |
| S0.2 Capture the pristine `ctest -L libshader` + `ctest -L visual` baseline | SC-003 demands before/after *per change*. If any stream lands first, its result becomes the next stream's "before" |
| S0.3 Capture the run-to-run **variance** baseline across the six `perf-manual` scenes on a quiescent machine, and the same-binary **image** noise floor | SC-004 and SC-006 are defined relative to a timing variance that does not exist yet; SC-007's "within noise" is undefined without the image floor |
| S0.4 Capture pre-change `perf-manual` ratios (same session as S0.3), the pre-change **emitted-form** evidence via S0.1's dump path, and the FR-006 uniform-in-conditional discrimination reference — all against the **unchanged** binary | Timing is only comparable within one quiescent measurement session; and a discrimination reference generated after the collapse would be a photograph of whatever the collapse produced, and so could never fail |

### Parallel streams (after Stream 0)

| Stream | Work | Parallel with | Serialization it imposes |
|---|---|---|---|
| **A — US1 (P1)** | Reproduce → **STOP** → narrowest fix → new shader/scene/reference → verify | B and C, for everything except the shared build | Touches `execute.cpp` or `rslo.y`; a landed fix invalidates B/C's baseline unless each stream re-verifies against its own captured "before" |
| **B — US2 (P2)** | `rslOps.cpp` callee audit (contract §2.2, not yet discharged) → uniform predicate in emitter → collapsed dispatch at every parity family → image verification → **exclusive-machine** timing | A and C for authoring and unit verification; **never** for timing | Emitter change invalidates every `.slo`; timing needs the machine idle |
| **C — US3 (P3)** | Converge light iteration onto one implementation (macro semantics) → before/after render set | A and B for authoring | Also invalidates `.slo`; shares the regeneration step with B |

Within a stream, the fine-grained `[P]` opportunities are: authoring the US1
probe shader and its scene pair alongside the reproduction work; drafting the
FR-012 documentation edits for all three streams concurrently; and, in US2,
applying the collapse family-by-family (`DEFOPCODE`/`DEFFUNC` arithmetic,
`DEFFUNC` builtins, `DEFSHORTFUNC` builtins) since each is an independent edit
against the same predicate.

### Hard serialization points (name these in `tasks.md`, never mark `[P]`)

1. **Stream 0 in full**, before anything lands.
2. **Every `perf-manual` timing run** — exclusive, quiescent machine. Nothing
   else compiles or renders during S0.3, S0.4, or US2's post-change run.
3. **`.slo` regeneration after any emitter or shading-runtime change**, with a
   `stat` check before every `-slo` verification. B and C cannot independently
   regenerate; whichever lands second regenerates once for both and re-verifies.
4. **The US1 STOP**, before any `.rslo` interpreter or compiler source edit.
   Also: within Stream B, the §2.2 callee audit precedes any collapse emitted
   at a family — the audit is a prerequisite, not a parallel task.
5. **A US3 STOP**, conditionally: only if implementation shows the shared entry
   point cannot preserve both backends' observable behaviour ([D6](./research.md)
   names the trigger). Absent that trigger, US3 proceeds under the FR-009
   refactor exemption.
6. **The FR-006 discrimination check must be authored before the collapse
   exists.** Its probe shader, scene pair, and reference image are produced in
   S0.4 against the unchanged binary; US2 only *runs* it. Authoring it after the
   collapse would bake the collapse's own output into the reference, leaving a
   check that passes unconditionally — including for the `n = 1` with live
   `tags` form that `contracts/op-uniform-collapse.md` §2.3 specifically
   forbids, which is exactly the failure this check exists to catch.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No violations. The Constitution Check passes on all seven principles — VII by
actually updating `docs/site/content/development/releases.md` under FR-012
(T052), not by exemption — and this feature adds no project, no dependency, no
build target, and no abstraction layer. Table intentionally empty.

## Post-Design Constitution Re-check

Re-evaluated after Phase 1 (`data-model.md`, `contracts/`, `quickstart.md`).

- **I. Clean Code** — still PASS. Phase 1 produced two contracts that *narrow*
  existing surfaces rather than adding any: the `op_*` collapse contract
  documents semantics of parameters that already exist, and the light-iteration
  contract describes the single entry point replacing two.
- **II. Language Standards** — still PASS, and now stronger: the collapse
  contract's central clause is that the C-linkage ABI does not change.
- **III. TDD** — still PASS. `quickstart.md` orders every validation
  Red-before-Green and makes the variance baseline an explicit prerequisite
  rather than an afterthought.
- **IV / V / VI** — unaffected by the design; no CLI surface, no dependency, no
  platform-specific path introduced.
- **VII** — still PASS, and now discharged by a concrete artifact rather than
  by argument: T052 writes the `docs/site/content/development/releases.md`
  entry and T053 verifies it landed. No exemption is claimed anywhere in this
  feature's artifacts.

**Result: PASS.** No new violations introduced by the design; Complexity
Tracking remains empty.
