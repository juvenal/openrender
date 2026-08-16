# Contract: Dynamic opcode-coverage guard (FR-006)

## Where it runs

A `ctest` target under the existing `-L libshader` label
(`ctest --test-dir build -L libshader`). It MUST NOT be invoked from any
render-time code path (`orender`, `oshader`, or any `op_*`/`rsl_*`
runtime function) — this is a hard requirement reinforced explicitly by
the user, independent of FR-006's original wording.

## Inputs (re-derived at test-run time, never a frozen list)

1. **Reachable set**: derived from `src/libshader/compiler/opcodes.cpp`'s
   canonical `DEFOPCODE`/`DEFFUNC` mnemonics, filtered to exclude
   Phase-0-triage-confirmed-unreachable entries (string-padding artifacts,
   the `gatherHeader`/`gatherhdr` case mismatch class, and any mnemonic no
   grammatically valid RSL program can actually cause the frontend to
   emit).
2. **Handled set**: the `kHandledOpcodes` table `llvmEmitter.cpp`'s
   `emitFunction()` consults (research.md D3) — read directly, not
   re-parsed from source text, so the test can never drift from what the
   emitter actually dispatches on.

Both sets are computed fresh on every test run — this is what makes the
guard "dynamic" per FR-006: an opcode added to `opcodes.cpp` after this
feature ships and reachable through the grammar, but never added to
`kHandledOpcodes`, fails this test without any manual update to the test
itself.

## Pass/fail contract

- **Pass**: reachable set ⊆ handled set.
- **Fail**: reachable set has at least one member not in the handled set.
  The failure message MUST name the specific missing mnemonic(s) — e.g.
  `"JIT coverage gap: opcode 'gather' is reachable but has no
  emitFunction() case"` — satisfying FR-006's "message identifying the
  specific unhandled construct" requirement. A bare "N opcodes missing"
  count without names does not satisfy this contract.

## Non-goals

- Does not check *correctness* of a handled opcode's codegen (that's the
  `-L visual` image-diff regression tests' job, covering FR-001/002/003/005
  parity).
- Does not run at build/compile time — a reachable-but-unhandled opcode
  still compiles (the emitter's fallback is a silent skip, not a build
  error); this guard is what makes that skip loud, but only when the test
  suite runs.
