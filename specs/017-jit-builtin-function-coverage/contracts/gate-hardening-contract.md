# Contract: JIT dispatch gate hardening (FR-008, FR-009)

This is the build-time half of this feature's two-layer defense against
the silent-skip defect class (see `function-coverage-guard-contract.md`
for the test-time half). It changes the *failure mode* of an unhandled
builtin function, not its detection mechanism — detection is unchanged
(`isHandledOpcode()` against `kHandledOpcodes[]`); what changes is what
happens when detection finds a gap.

## Where it runs

Inside `oshader --jit`'s compilation pipeline — `emitFunction()`
(`llvmEmitter.cpp:509`) via its caller `emitLLVMBitcode()`
(`llvmEmitter.cpp:2346`), invoked from `oshader.cpp`'s `main()` whenever a
`.sl` file is compiled with `--jit`. Fires at `.slo` compile time, for
every shader compiled this way — including as part of the normal
`cmake --build` (`shaders/CMakeLists.txt` compiles every `.sl` to `.slo`
unconditionally when the JIT is enabled).

## Before this feature (current, silent-skip behavior)

`emitFunction()`'s coverage gate (`llvmEmitter.cpp:826`):
```cpp
if (!isHandledOpcode(op))
    continue;
```
An unhandled builtin function's IR instruction is skipped: no LLVM IR
emitted for that call, no diagnostic, `emitFunction()` continues to the
next instruction, `emitLLVMBitcode()` returns `true` (verification passes,
since nothing invalid was emitted — just nothing at all), `.slo` compiles
successfully and is written to disk with that function's effect silently
missing.

## After this feature (US2, hardened behavior)

The same gate instead records a hard failure — naming the specific
unhandled mnemonic — that propagates up through `emitFunction()` and
causes `emitLLVMBitcode()` to return `false`, exactly as it already does
for an IR-verification failure (`llvmEmitter.cpp:2405-2414`).
`oshader.cpp:431-435`'s existing caller code requires no change:
```cpp
const bool ok = emitLLVMBitcode(*currentCompiler->lastCompiledModule, sloPath, currentCompiler->shaderName);
if (!ok)
    error = ERR_COMPILE;
```
`error` becomes `oshader`'s process exit code (`ERR_COMPILE = 3`,
`oshader.cpp:61`, returned from `main()` at line 482) — `oshader --jit`
exits nonzero, prints a diagnostic naming the unhandled mnemonic (stderr,
matching the existing IR-verification-failure diagnostic's format
convention), and produces **no** `.slo` output file for that shader.

## Pass/fail contract

- **Pass** (no unhandled mnemonic present): identical behavior to today —
  `.slo` compiles and is written, zero observable change.
- **Fail** (unhandled mnemonic present): `oshader --jit` exits nonzero
  (`ERR_COMPILE`), prints a diagnostic naming the specific unhandled
  mnemonic to stderr, and writes no `.slo` file. This is a **build-time**
  failure — if triggered during `cmake --build` (via
  `shaders/CMakeLists.txt`'s unconditional per-`.sl` `--jit` compile
  step), the overall build fails.

## Sequencing requirement (FR-009) — load-bearing, not optional

This gate MUST NOT be hardened until every builtin function any
currently-shipped shader calls has JIT support — i.e., not before User
Story 1's six functions (`visibility`/`transmission`/`trace`/`occlusion`/
`indirectdiffuse`/`comp`) are implemented and verified. Confirmed by
direct grep (`research.md` D4) that hardening strictly after User Story 1
causes zero build breakage, both immediately and for the entire duration
User Stories 3/4 remain unimplemented (zero shipped shaders reference any
of the remaining 19 functions). `tasks.md` MUST NOT reorder this — the
gate-hardening task is the last task of User Story 2, which itself is
sequenced after User Story 1 completes.

## Verification obligations

1. **Positive (no breakage)**: `cmake --build build` from a clean tree
   succeeds with zero new failures after this change lands (FR-009/SC-004)
   — proves every currently-shipped `.sl` file still compiles to `.slo`.
2. **Negative (gate actually fires)**: a dedicated test — NOT part of the
   normal shader build — that deliberately compiles a fixture `.sl` file
   calling a builtin function confirmed to have no JIT handling (or a
   synthetic/nonexistent mnemonic, for a check that remains valid even
   after this feature eventually closes every real gap) via `oshader --jit`,
   and asserts: nonzero exit code, a diagnostic naming that specific
   mnemonic on stderr, and no `.slo` file produced. This is the acceptance
   test for User Story 2's Acceptance Scenario 1 in spec.md, and should be
   written and shown to fail against the *pre-hardening* gate (TDD red
   phase, constitution Principle III) before the hardening change lands.

## Non-goals

- Does not replace the `function-coverage-guard-contract.md` test-time
  guard — the two are independent, complementary layers (this contract's
  own opening paragraph). A future regression that somehow bypasses this
  build-time gate (e.g. a shader that's never recompiled after a bug is
  introduced) is still caught by the test-time guard on the next
  `ctest -L libshader` run.
- Does not change `oshader`'s behavior for any *already-handled* mnemonic
  — zero blast radius on anything currently working (`research.md` D4).
- Does not add a new opt-out/suppression flag for this failure — an
  unhandled builtin function under `--jit` is always a hard error post-US2;
  the existing `Attribute "shade" "shaderformat" ["rslo"]` /
  `Option "shaderformat" "default" ["rslo"]` mechanisms remain the correct,
  unaffected way to opt a specific shader/scene out of the JIT path
  entirely (compiling it as `.rslo` only, never invoking `--jit` for it in
  the first place).
