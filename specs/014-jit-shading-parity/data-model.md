# Phase 1 Data Model: JIT/Interpreter Shading Parity Fixes

This feature has no persistent storage or user-facing data model. The
"entities" below are the in-process/compile-time/load-time structures this
feature reads, fixes, or compares, derived from spec.md's Key Entities
section.

## `usedParameters` bitmask

A per-compiled-shader set of flags recording which runtime behaviors a
given shader's execution needs — one bit per RSL global variable
(`PARAMETER_CI`, `PARAMETER_OI`, `PARAMETER_OL`, ...) plus a handful of
behavior flags (`PARAMETER_RAYTRACE`, `PARAMETER_MESSAGEPASSING`,
`PARAMETER_NONAMBIENT`, the derivative family). Consumed at every render by
the shared runtime (`shading.cpp`'s `complete()`, both overloads) to decide
whether to run default-fill, derivative-footprint, raytrace-aware, and
message-passing-locals logic.

| Field | Type | Source of truth | Notes |
|---|---|---|---|
| bit values | `unsigned int` constants | `src/ri/rendererc.h` (`PARAMETER_*` #defines) | Shared by both backends; not modified by this feature. |
| interpreter-computed value | `int` field, `CShader::usedParameters` (`src/libshader/shading/shader.h:151`) | Computed by `shading/rslo.y` at `.rslo`-load time, driven by live `CRenderer` declaration state (`retrieveVariable`) | Reference implementation (research.md D4) — this feature does not change how this value is computed, only two narrow interpreter bugs elsewhere (FR-007/FR-008). |
| JIT-computed value | same field, populated via `.slo` bitcode metadata | Computed once by `llvmEmitter.cpp` at `oshader --jit` compile time, embedded as LLVM metadata, deserialized (never recomputed) at `.slo`-load time by `llvmJitMetadata.cpp:111-117` | This is what FR-001–FR-005 fix — today wrong (Story 1/2) because the compile-time computation uses the wrong gating condition, not because the storage/consumption mechanism differs from the interpreter's. |

**Critical asymmetry** (research.md D6/D7): the interpreter recomputes this
value fresh on every `.rslo` load; the JIT computes it once at `oshader
--jit` time and never again. This is why the FR-012 differential test must
compile the *same* `.sl` source through both backends and load both
results through the real runtime, rather than comparing static tables —
see the Regression-Guard Test Matrix below.

## Global-Variable Bit Table (`kParamBits`, fixed not replaced)

| Field | Type | Notes |
|---|---|---|
| `name` | `const char*` | RSL global name (`Ci`, `Oi`, `du`, `dv`, ...). |
| `bit` | `unsigned int` | The `PARAMETER_*` constant it maps to. Values independently confirmed correct by the originating investigation (research.md D1) — only the *gating condition* that decides whether a name counts as "referenced" was wrong. |

**Fix** (FR-001): gate on actual `result`/`operand` name matches within
`IRModule.initFn`/`codeFn` instructions, not on `v.slcType & SLC_GLOBAL`
(true for the entire pre-seeded global table regardless of shader content).

## Opcode/Function Bit Table (new, X-macro re-expanded)

| Field | Type | Source of truth | Notes |
|---|---|---|---|
| `(text, params)` | `(const char*, unsigned int)` | Re-expanded via local X-macro redefinition of `DEFOPCODE`/`DEFFUNC`-style macros over `shaderOpcodes.h`/`shaderFunctions.h`/`giOpcodes.h`/`giFunctions.h` (research.md D3), mirroring the existing `rslo_code.h:31-49` precedent | Subsumes FR-002 (raytrace), FR-003 (message-passing), FR-004 (non-ambient, corrected to exclude `illuminance`/`endilluminance`), and the builtin half of FR-005 (derivative-tagged builtins). |

**Invariant enforced by the table-parity guard (tier 1, research.md D6)**:
this table's `(text, params)` pairs MUST stay byte-identical to the
interpreter headers it re-expands, by construction (re-`#include`, not
hand-transcription) — see `contracts/table-parity-contract.md`.

## `computeUsedParameters` (new combined scan)

The single function `llvmEmitter.cpp` calls in place of today's two-part
logic (declared-scope variable scan + narrow 3-opcode `hasNonAmbientOp`
check).

| Field | Type | Notes |
|---|---|---|
| input | `const IRModule&` | Scans `initFn` and `codeFn` instructions once. |
| output | `unsigned int` | The corrected `usedParameters` bitmask, embedded as `.slo` LLVM metadata. |
| mechanism | combined name-match (global table) OR opcode-match (function table) | Research.md D2 — a single pass, not two separate fixes, so the derivative-via-builtin regression named in Story 2 Acceptance Scenario 4 cannot reappear as a sequencing gap. |

## Regression-Guard Test Matrix (research.md D6)

| Tier | What it checks | Link scope | Catches original bug? |
|---|---|---|---|
| 1. Table-parity guard | X-macro table == interpreter headers, byte-for-byte | `libshader_compiler` only | No (values were always correct — see D1) |
| 2. Gating-condition unit tests | `usedParameters` bits for minimal `.sl` sources compiled through the real emitter | `libshader_compiler` only | Yes — this is the primary guard for FR-001–FR-005's actual bug class |
| 3. Live differential oracle (FR-012) | `CShader::usedParameters` for the same source, loaded via `CRenderer::context->getShader()` for both `.slo` and `.rslo` | `ri` + `libshader_shading` (mirrors `tests/imager/`) | Yes — literal, end-to-end satisfaction of FR-012's wording |

## Ambient `Cl`/`Ol` Accumulation (User Story 3, FR-007/FR-008)

Not a new entity — two narrow interpreter-side bug fixes to existing state
(`ss->alights->savedState[1]`, accumulated during `execute()`'s `execEnd:`
block and `shading.cpp`'s `prepareAmbient()`/`callAmbient()`). No schema
change; see spec.md FR-007/FR-008 and plan.md's Project Structure tree for
the exact call sites.

## `s_rslGlobals` / `Ol` wiring (User Story 4, FR-009/FR-010)

Investigative entities, not yet a confirmed fix shape — `data-model.md`
does not prescribe the collapse mechanism ahead of the read/compare pass
research.md D5 calls for; the determination and (if applicable) resulting
single-source-of-truth structure will be recorded in `tasks.md` and the
implementation itself, not designed speculatively here.
