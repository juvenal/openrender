# Contract: JIT runtime C-linkage entry points (`rslOps.h`, `rslBuiltins.h`)

These functions are the ABI between `oshader --jit` bitcode and `libshader_shading`. Any change
recompiles every `.slo`, because rebuilding `oshader` rebuilds the shaders (CLAUDE.md: dependency
tracking is real).

## Added

```c
/* solar() with no arguments. With the illumination hook active, calls solarBegin(NULL, NULL).
 * Otherwise mirrors SOLAR1EXPR_PRE: a tagged point gets its tag incremented, an active one
 * gets L = Ps. No culling. */
void op_solar0_begin(int *tags, int n, int *numActive, int *numPassive);
```

- **Emitted** by `llvmEmitter.cpp` for a `solar` instruction whose only operand is its label.
- **Implemented** in `rslOps.cpp` as a call to `activeContext()->jitSolar0Begin(...)`.
- **Symbol resolution**: like every `op_*`, it is resolved when the JIT binds the shader, through
  the process search generator (CLAUDE.md gotcha 3). If it fails to resolve, wire up
  `addProcessSymbol()` as that note says.
- **Opcode coverage**: the mnemonic `solar` is already covered (`LibShader_OpcodeCoverage`). No
  new mnemonic is added.

## Changed behaviour, same signatures

`op_illuminate_begin`, `op_illuminate3_begin`, `op_solar_begin`, `op_illuminate_end` and
`op_solar_end` (through their `jit*` methods) now check the illumination hook first
([illumination-hook](illumination-hook.md)). Their signatures do not change.

## Removed (uncalled, FR-011)

These are deleted from `rslBuiltins.{h,cpp}`:
- `int rsl_illuminate_begin_1(const float *Pl)`
- `int rsl_illuminate_begin_3(const float *Pl, const float *Nf, const float *theta)`
- `void rsl_illuminate_end(void)`
- `int rsl_solar_begin_1(void)`
- `int rsl_solar_begin_2(const float *Nf, const float *theta)`
- `void rsl_solar_end(void)`

No emitter, test, generated file or document references them. The registry text mentioning them
(DB-012 evidence) moves to Resolved with that entry.

## Emitter hardening (required; approved 2026-10-14)

If an `illuminate`/`solar` instruction's operands do not resolve to one of the four known forms
([illumination-hook](illumination-hook.md)), compiling with `oshader --jit` fails with an error
naming the mnemonic. A negative test asserts the error, as `LibShader_GateHardening` does for the
opcode gate. Today that case is skipped
silently, which is how the `solar()` gap went unnoticed. Spec 017 hardened `kHandledOpcodes[]` the
same way.
