# Contract: JIT-callable `op_*` wrapper ABI

This is the internal contract every new `op_*` function added by this
feature (`op_cfrom`, `op_mfrom`, `op_ctransform`, and any Phase 3 additions)
must satisfy. It is the "interface" a library feature like this exposes —
not a network/CLI contract, but the calling convention between
JIT-generated code (`llvmEmitter.cpp`) and the runtime wrapper
(`rslOps.cpp`).

## Signature convention

- C linkage (`extern "C"` or equivalent), matching every existing `op_*`/
  `rsl_*` function in `rslOps.h`.
- Parameter shape mirrors the source `DEFOPCODE`/`DEFFUNC` operand list.
  For the `cfrom`/`mfrom`/`pfrom` family (3-operand: dst, space-string,
  src), the template is `op_pfrom` (`rslOps.h:224`, `rslOps.cpp:493`):

  ```c
  void op_pfrom(float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags);
  ```

  `op_cfrom`/`op_mfrom`/`op_ctransform` follow this exact shape, varying
  only in element width (color/matrix vs. point) where the underlying
  `DEFOPCODE` differs.

## Delegation requirement (FR-007)

The wrapper body MUST:
1. Resolve any coordinate/color-system name via
   `libshader::activeContext()->jitFindCoordinateSystem(space, from, to, cSystem)`
   (`shading.cpp:2481`) — the same trie the interpreter uses. Wrappers that
   don't need a space lookup (e.g. pure matrix arithmetic) skip this step.
2. Call the single interpreter-shared function identified in
   `data-model.md`'s `RSL Construct.interpreter_target` for that mnemonic —
   e.g. `convertColorFrom`/`convertColorTo` (now in `src/common/colorSpace.h`,
   see `research.md` D2) for `cfrom`/`ctransform`.
3. MUST NOT contain any new arithmetic/shading logic beyond marshaling
   between the JIT's flat `float*` buffers and the delegated function's
   parameters (loop over active vertices/tags as `op_pfrom` already does).

## Emitter-side pairing

`llvmEmitter.cpp`'s `emitFunction()` case for the mnemonic must:
- Be keyed off the shared `kHandledOpcodes` table (see
  `coverage-guard-contract.md`), not a bare string literal in an
  `if/else-if` chain.
- Call `declareOp(mod, "op_<mnemonic>", ty)` + `B.CreateCall(...)` —
  the same pattern every existing handled opcode uses (zero
  `CreateFAdd`/`CreateFMul`/etc. raw IR construction anywhere in this
  file, confirmed during investigation).

## Verification obligation

Each new `op_*` symbol must be confirmed to resolve at JIT bind time via
`DynamicLibrarySearchGenerator::GetForCurrentProcess()` (the existing
mechanism the 8 current `.slo` visual tests already rely on). Only add
`jitSymbolRetain.cpp`-style explicit retention if one doesn't resolve —
verify-on-add per research.md D5, not a blanket prerequisite.
