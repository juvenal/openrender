# Contract: JIT-callable `op_*` wrapper ABI

This is the internal contract every new `op_*` function this feature adds
(across US1/US3/US4) must satisfy. It is the "interface" a library feature
like this exposes — not a network/CLI contract, but the calling convention
between JIT-generated code (`llvmEmitter.cpp`) and the runtime wrapper
(`rslOps.cpp`).

## Signature convention

- C linkage (`extern "C"` or equivalent), matching every existing `op_*`/
  `rsl_*` function in `rslOps.h`.
- Parameter shape mirrors the source `DEF*FUNC` operand list, plus `n`
  (vertex count) and `tags` (active-vertex mask) trailing parameters,
  matching every existing `op_*` convention (e.g. `op_reflect`,
  `op_maxf`).
- **Pure-operand functions** (all of US4, plus `comp`/`ptlined`): no
  `CShadingContext` involvement. Template: `op_reflect`
  (`rslOps.h:...`, `rslOps.cpp:1338`):
  ```c
  void op_reflect(float *dst, int sd, const float *I, int si, const float *N, int sn, int n, const int *tags);
  ```
- **Context-needing, non-raytracing functions** (`raylabel`, `raydepth`,
  `rayinfo`): fetch `CShadingContext *ctx = libshader::activeContext();`
  and delegate. Template: `op_shadow_f` (`rslOps.cpp:1583-1588`):
  ```c
  void op_shadow_f(float *dst, int sd, ..., int n, const int *tags) {
      CShadingContext *ctx = libshader::activeContext();
      if (ctx) ctx->jitShadowF(...);
  }
  ```
- **Raytracing-tier functions** (`visibility`, `transmission`, `trace`,
  `occlusion`, `indirectdiffuse`, `photonmap`): same `activeContext()`
  delegation shape, but the wrapper ALSO receives `du`/`dv`/`N`/`time`
  grid pointers (loaded by the emitter via `resolveVar`+`loadVarPtr`, see
  `research.md` D2) to pass through to the `CShadingContext` method:
  ```c
  void op_visibility(float *dst, int sd, const float *P, int sp,
                      const float *D, int sD, const float *du,
                      const float *dv, const float *N, const float *time,
                      int n, const int *tags) {
      CShadingContext *ctx = libshader::activeContext();
      if (ctx) ctx->jitVisibility(dst, sd, P, sp, D, sD, du, dv, N, time, n, tags);
  }
  ```
  (Exact parameter list finalized per-function at implementation time,
  following whichever operands that function's `giFunctions.h` macro
  family actually consumes.)

## Delegation requirement (FR-016)

The wrapper (and, for raytracing-tier functions, the `CShadingContext`
method it delegates to) MUST:
1. For US4's pure-math/string functions: call the identified
   `mathSpec.h` free function (or, for `match`, alias directly to the
   existing `op_seql`) — see `research.md` D5's table for each function's
   specific target.
2. For US1's raytracing tier and `photonmap`: transcribe the corresponding
   `giFunctions.h` macro family byte-faithfully inside a new
   `CShadingContext` member method (`research.md` D2), including the
   `numRealVertices`-bounded-then-replicate loop discipline (`research.md`
   D1) — this is new code by necessity (no existing function to call), but
   its *behavior* must match the interpreter's macro exactly, not
   approximate it.
3. MUST NOT contain any new arithmetic/shading/raytracing logic beyond
   what's required to reproduce the interpreter's exact behavior — no
   "improved" or "corrected" semantics (see `research.md` D5's `match`/
   `round`/`min` notes — mirror the interpreter's current behavior exactly,
   quirks included, per spec.md FR-017).

## Emitter-side pairing

`llvmEmitter.cpp`'s `emitFunction()` case for each mnemonic must:
- Add the mnemonic to `kHandledOpcodes[]` (alphabetically, matching the
  array's existing ordering convention).
- Call `declareOp(mod, "op_<name>", ty)` + `B.CreateCall(...)` — the same
  pattern every existing handled opcode uses (zero raw
  `CreateFAdd`/`CreateFMul`/etc. IR construction for a builtin-function
  call site anywhere in this file, confirmed during investigation).
- For raytracing-tier functions: resolve `du`/`dv`/`N`/`time` via
  `resolveVar("du", d)` + `loadVarPtr(d)` (and similarly for the others) —
  already-generic machinery, zero new global-table entries needed
  (`research.md` D2).
- For multi-form mnemonics (`comp`/`min`/`trace`/`setcomp`): branch on
  `dstDesc.stride` and/or operand count to select the variant, exactly
  like the existing `"noise"`/`"snoise"` dispatch
  (`llvmEmitter.cpp:1808-1822`) does for its own f-vs-v selection.

## Verification obligation

Each new `op_*` symbol must be confirmed to resolve at JIT bind time via
`DynamicLibrarySearchGenerator::GetForCurrentProcess()` (the existing
mechanism every current `.slo` visual test already relies on). Only add
`jitSymbolRetain.cpp`-style explicit retention if one doesn't resolve —
verify-on-add, not a blanket prerequisite (matches spec 011's precedent,
`CLAUDE.md` gotcha #3).
