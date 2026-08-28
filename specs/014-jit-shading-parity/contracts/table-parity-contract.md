# Contract: Table-parity guard (tier 1, supports FR-001–FR-005)

## Where it runs

A `ctest` target under the existing `-L libshader` label
(`ctest --test-dir build -L libshader`). `libshader_compiler`-only link —
no `ri`, no `libshader_shading` link required.

## Inputs (re-derived at test-run time, never a frozen list)

1. **Compiler-side table**: the `(text, params)` pairs `llvmEmitter.cpp`'s
   new X-macro re-expansion produces from `shaderOpcodes.h`/
   `shaderFunctions.h`/`giOpcodes.h`/`giFunctions.h` (research.md D3).
2. **Interpreter-side table**: the same headers, read directly (not
   re-parsed from source text) — since the compiler-side table is itself
   produced by `#include`-ing these headers with locally redefined macros,
   this tier is closer to "confirm the re-expansion mechanism itself
   compiles and produces the expected shape" than an independent
   comparison — the X-macro technique makes drift structurally impossible
   for the *table*, but does not by itself confirm the *gating condition*
   that consumes the table is correct (see the gating-condition-unit-tests
   contract for that).

## Pass/fail contract

- **Pass**: every `(text, params)` pair in the compiler-side table matches
  the corresponding interpreter header entry exactly.
- **Fail**: a mismatch was possible only if a future edit hand-overrides
  part of the X-macro re-expansion (e.g. reverting to the fallback
  hand-transcription path documented in research.md D3) and the
  hand-transcribed copy drifts from the header it was meant to mirror. The
  failure message MUST name the specific mnemonic/bit pair that disagrees.

## Non-goals

- **Does not, by itself, guard against the original bug class.**
  Research.md D1 confirms the pre-fix `kParamBits` table's *values* were
  always correct — the bug was in the *gating condition* deciding whether a
  shader "uses" a given entry, which this tier cannot see. This tier is a
  necessary complement to, never a substitute for, the gating-condition
  unit tests (`contracts/gating-condition-contract.md`).
- Does not exercise the interpreter's live `.rslo`-load-time computation at
  all (see `contracts/differential-oracle-contract.md` for that).
