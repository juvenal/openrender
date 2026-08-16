# Phase 1 Data Model: LLVM JIT Opcode-Coverage Parity Sweep

This feature has no persistent storage or user-facing data model. The
"entities" below are the in-process/build-time structures this feature
introduces or manipulates, derived from spec.md's Key Entities section.

## RSL Construct

An RSL language construct with a canonical opcode mnemonic (e.g. `cfrom`,
`mulmm`, `gather`).

| Field | Type | Source of truth | Notes |
|---|---|---|---|
| `mnemonic` | string | `src/libshader/compiler/opcodes.cpp` `DEFOPCODE`/`DEFFUNC` entries | Canonical name; some entries are string-padded (e.g. `"\tcfrom             "`) and must be trimmed before comparison. |
| `operand_shape` | tuple | same `DEFOPCODE` declaration | e.g. `cfrom`/`mfrom`/`pfrom` are all 3-operand (dst, space-string, src). |
| `interpreter_target` | function/macro reference | `shaderOpcodes.h` / `shaderFunctions.h` | The macro (e.g. `CFROMEXPR`) and the function it expands to (e.g. `convertColorFrom`) — this is what a JIT wrapper must delegate to per FR-007. |
| `reachable` | boolean | Phase 0 triage (compiled `.sl` repro + IR text grep) | True only if a real, grammatically valid RSL program causes the frontend to actually emit this mnemonic — distinct from merely appearing in `opcodes.cpp`. |

## Reachability Inventory

The Phase 0 triage's output: the authoritative list of RSL constructs
confirmed `reachable == true`, replacing the speculative ~48-candidate raw
diff. Lives in this feature's `research.md`/`tasks.md` (not a runtime
structure) until Phase 2, where it becomes the coverage guard's expected
set.

| Field | Type | Notes |
|---|---|---|
| `mnemonic` | string | Matches `RSL Construct.mnemonic`. |
| `category` | enum | One of: color/matrix-space (`cfrom`/`mfrom`/`ctransform`), matrix arithmetic, GI (`gather` family), comparison/logic, array move. Mirrors spec.md's User Stories 1/3. |
| `triage_repro` | file reference | The minimal `.sl` file + IR-text grep used to confirm reachability; retained as a regression fixture. |

## Coverage Guard Table (`kHandledOpcodes`)

The single source of truth `emitFunction()` and the new coverage-guard
ctest both consult, per research.md D3. Concrete shape (finalized at
implementation time, not prescribed further here — see `contracts/`):

| Field | Type | Notes |
|---|---|---|
| `mnemonic` | `const char*` | Matches `RSL Construct.mnemonic` after trimming. |
| (dispatch behavior) | function pointer / switch case | `emitFunction()`'s existing per-opcode codegen, refactored to be keyed off this table rather than a bare `if/else-if` string chain. |

**Invariant enforced by the coverage-guard test (FR-006)**: for every
`RSL Construct` where `reachable == true`, `mnemonic` MUST appear in
`kHandledOpcodes`. Violation fails the test with a message identifying the
specific missing mnemonic — never silently skipped, and never checked at
render runtime.

## `op_*` JIT Wrapper (new instances)

Each new JIT-callable delegation wrapper added by this feature
(`op_cfrom`, `op_mfrom`, `op_ctransform`, plus whatever Phase 3 triage
confirms for matrix arithmetic / `gather()` / comparison-logic / array
move ops).

| Field | Type | Notes |
|---|---|---|
| `name` | string | `op_<mnemonic>` naming convention, matching existing `op_pfrom`/`op_specular_batch`. |
| `signature` | C-linkage function signature | Follows the operand shape of the `RSL Construct` it implements (see `op_pfrom`, `rslOps.h:224`, as the template). |
| `delegates_to` | function reference | MUST be the same function/macro-target the interpreter's `shaderOpcodes.h`/`shaderFunctions.h` macro already calls (FR-007) — never new math. |
| `emitter_case` | `llvmEmitter.cpp` dispatch entry | The `declareOp(mod, "op_...", ty)` + `B.CreateCall(...)` pairing that invokes this wrapper from JIT-compiled IR. |

## Relocated Shared Function (`src/common/colorSpace.{h,cpp}`)

| Field | Value |
|---|---|
| `functions` | `convertColorFrom(float*, const float*, ECoordinateSystem)`, `convertColorTo(float*, const float*, ECoordinateSystem)` |
| `previous_location` | `src/ri/init.cpp:67,228` |
| `new_location` | `src/common/colorSpace.cpp`, declared in `src/common/colorSpace.h`, built into `openrendercommon` |
| `callers_updated` | `execute.cpp` (interpreter, existing), `shaderOpcodes.h`/`shaderFunctions.h` macros (interpreter, existing — via updated include), `rslOps.cpp`'s new `op_cfrom`/`op_ctransform` (JIT, new) |
| `behavior_change` | None — verbatim relocation, satisfies FR-009. |
