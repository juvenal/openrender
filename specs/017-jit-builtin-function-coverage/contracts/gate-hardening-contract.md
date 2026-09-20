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
User Stories 3/4/5 remain unimplemented (zero shipped shaders reference
any of the remaining 37 functions — 19 from Stories 3/4, plus 18 more from
Story 5, discovered mid-implementation and confirmed zero-shipped-impact
by the same direct-grep method, `research.md` D9). `tasks.md` MUST NOT
reorder this — the gate-hardening task is the last task of User Story 2,
which itself is sequenced after User Story 1 completes.

## Verification obligations

1. **Positive (no breakage)**: `cmake --build build` from a clean tree
   succeeds with zero new failures after this change lands (FR-009/SC-004)
   — proves every currently-shipped `.sl` file still compiles to `.slo`.
2. **Negative (gate actually fires)**: a dedicated test — NOT part of the
   normal shader build — that deliberately triggers the coverage gate and
   asserts: failure, a diagnostic naming the specific mnemonic on stderr,
   and no `.slo` file produced. This is the acceptance test for User
   Story 2's Acceptance Scenario 1 in spec.md, and was written and shown
   to fail against the *pre-hardening* gate (TDD red phase, constitution
   Principle III) before the hardening change landed.

   **Implementation history**: initially built as `oshader --jit` shelled
   out against a fixture `.sl` calling a real builtin confirmed to have
   no JIT dispatch case yet (`fixtures/gate_hardening_probe.sl`),
   repointed at each successive target as US3/US4 closed real gaps
   (degrees → step → setcomp → rotate → concat → format). That approach
   hit exactly the dead end this obligation's original wording already
   flagged as a risk ("a check that remains valid even after this
   feature eventually closes every real gap"): once `format()` landed as
   the last function in `kAllFunctionMnemonics` (the full builtin-
   function universe per `function-coverage-guard-contract.md`, not just
   this spec's original 26-function inventory) to gain a dispatch case,
   there was no longer any real, compilable RSL builtin left to serve as
   the fixture. Redesigned (2026-09-20, `test_gate_hardening.cpp`) to
   link `libshader_compiler` directly and drive `emitLLVMBitcode()` with
   a hand-built `IRModule` containing a synthetic opcode mnemonic
   (`"__unhandled_test_opcode__"`) that can never collide with a real
   one — testing `isHandledOpcode()`'s actual contract instead of a
   proxy for it, so it can never go stale as coverage grows. The fixture
   `.sl` was deleted as unused. A positive-control assertion (the same
   `IRModule` shape using only already-handled opcodes must compile
   successfully and write a `.slo`) precedes the negative one, so a
   malformed harness can't produce a false "gate fired" result.

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
