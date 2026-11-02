# Contract: the JIT's DSO call path (FR-020-FR-022, US6)

Research M11, M12, R12-R14.

## Compiler side (`src/libshader/compiler/llvmEmitter.cpp`)

The IR is unchanged. The `.rslo` text of a DSO call is

```text
DSO <name> ("<prototype>") [<destination>] <argument>...
```

and `CIRBuilder::parseLine` stores it as opcode `DSO`, result `<name>`, operands `("<prototype>")`,
`[<destination>]`, `<argument>...`; the destination is present unless the prototype starts with
`o`. One helper decodes that form; nothing else in the emitter reads a `DSO` instruction's fields.

**The destination prelude must not run for `DSO`.** Before the coverage gate, `emitFunction`
resolves `ins.result` as the destination (`llvmEmitter.cpp:1101-1105`: `hasDst`, `dst`,
`dstStrideVal`, which falls back to 3 when nothing resolves). For a `DSO` line `ins.result` is the
operand's *name*: usually nothing resolves and the fallback stride 3 would make a uniform call look
varying in `collapseArgs`; a shader variable with the operand's name would even be taken as the
destination. So the `DSO` case is dispatched before that prelude (or the prelude is skipped for it):
its destination is the decoded `operands[1]` (value-returning only), and `collapseArgs` receives the
decoded destination's and operands' strides only.

Per function (`Init` and `Code`) the emitter:
1. assigns each distinct (name, prototype) a slot k, in first-use order across the module;
2. emits, only if the module has at least one DSO call,
   - `@"<shader>.shadeops" = global [N x ptr] zeroinitializer` (external linkage: the loader looks it
     up; the shader name keeps it unique in the shared JIT dylib),
   - named metadata `openrender.shader.shadeops`: N nodes `!{!"<name>", !"<prototype>"}` in slot order,
   - the format record 2.1.0 (Q1; 2.0.0 for every other module);
3. lowers each call to

```c
void op_dso_call(void *slotEntry,    /* the CDSO * loaded from @"<shader>.shadeops"[k] */
                 int isVoid,         /* prototype starts with 'o' */
                 int inInit,         /* 1 in the init function */
                 int argc,           /* operands after the destination */
                 void **argv,        /* argc + 1 pointers: [0] destination or nullptr, then the operands */
                 const int *steps,   /* argc + 1 per-vertex byte sizes (VarDesc item bytes), [0] 0 for void */
                 int n,              /* from collapseArgs: 1 when every operand is uniform */
                 const int *tags);   /* from collapseArgs: nullptr on the uniform path */
```

with `argv`/`steps` built in stack arrays (`alloca`) at the call site.

`kHandledOpcodes` gains `"DSO"`. The two `"XXX"` rows stay out of the coverage guard's universe.

## Runtime side (`src/libshader/shading/rslOps.{h,cpp}`)

`op_dso_call` reproduces the interpreter's `DEFFUNC` loop for the DSO rows (`execute.cpp:601-625`,
`scriptFunctions.h:1136-1193`):
- `operand = ((CDSO *)slotEntry)->operand`; `exec = shadeopOperandExec(operand)`;
  `data = shadeopOperandData(operand, inInit ? 0 : libshader::activeContext()->thread)`;
- `n == 1 && tags == nullptr`: one `exec(data, argc, argv)`;
- otherwise, for each of the `n` vertices: if `tags == nullptr || tags[v] == 0`, `exec(data, argc, argv)`;
  then, for every vertex, advance each `argv[i]` (i ≥ 1, and 0 when not void) by `steps[i]` bytes.

`argv[0]` is the destination for a value-returning operand and unused for a void one, as in the
interpreter (`DSOEXEC`: `exec(handle, numArguments - 1, op)`; `DSOVOIDEXEC`: `exec(handle,
numArguments, op)` with `op[0]` unused). The function is defined in `libshader_shading`, resolved by
the JIT through the process (gotcha 3), and calls only `libRIBShadeop`, which the library links: no
flat-namespace import is added.

## Loader side

- `CLLVMJitEngine::compileShader` (`llvmJit.cpp:132`) records, per shader name, the slot list read
  from `openrender.shader.shadeops` and, after `addIRModule`, the address of `@"<shader>.shadeops"`
  (`jit->lookup`). Cached with the module.
- New `CLLVMJitEngine::bindShadeops(shaderName, resolve)`: for each slot in order,
  `entry = resolve(name, prototype)`; a `nullptr` stops and returns the failing slot.
- `parseSloShader` (`sloLoader.cpp:139`) calls it with `libshader::lookupDSO` on every load, after
  `compileShader` returns (the module cache outlives a `RiBegin`/`RiEnd` session; the operands live
  until `RiEnd`). On a missing operand (Q2, approved): `warning(CODE_BADFILE, "Error in shader
  \"%s\" (\"Failed to locate DSO function %s (%s)\") (v%d.%d.%d, .slo format %d.%d.%d)\n", ...)` and
  `return nullptr`, so `getShader` falls back to the `.rslo` (`rendererFiles.cpp:395-417`).
- A module with no `openrender.shader.shadeops` node binds nothing (every `.slo` written before this
  spec, and every one without DSO calls).

## Format and versions

- `common/sloFormat.h`: `SLO_FORMAT_MINOR` 0 → 1 (what the loader reads, and what a module with a
  DSO call records); new `SLO_FORMAT_MINOR_BASE 0` (what every other module records, so their bytes
  stay); the comment says the record is the lowest reader version the module needs; history line
  "2.1 spec 038: DSO shadeop calls (`op_dso_call`, the `openrender.shader.shadeops` slots)". The
  emitter writes `SLO_FORMAT_MINOR` only when the module has a slot (`llvmEmitter.cpp:502-503`).
- `LibShader_Format_slo_emitted` (`test_format_versions.cpp:276-300`) expects the base record for its
  probe; a new case expects 2.1.0 from a probe with a DSO call (Q1 approved; TEST-APPROVAL). The `newer_minor`
  refusal (`:318`, `SLO_FORMAT_MINOR + 1`) keeps its meaning.
  A 2.0 loader rejects a 2.1 module before linking; a 2.0 module loads in a 2.1 loader.
- `CRendererServices` stays 2.0.0 (Q8, changed by the maintainer: a comment-only change gets no bump):
  `getDSO`'s comment gains the `.slo` loader and the version-history comment in `rendererServices.h`
  records the wording change; no member changes, no version constant changes.
- Shader DSO Operands interface (`shadeop.h` 1.1.0): unchanged.

## Tests (FR-022, SC-008), all comparing `.rslo` against `.slo` renders in the same run

| Case | Shader (new, `tests/plugins/shadeop/`) | Expectation |
|---|---|---|
| uniform value-returning call | `probe.sl` (existing) compiled with `--jit` too | 0 differing pixels |
| two operands of one module | `probe_two.sl` (existing) with `--jit` | 0 differing pixels |
| varying, uniform-promoted, string and masked arguments | `probe_args.sl` | 0 differing pixels, including the points the `if` masks |
| void operand | `probe_void.sl` with a new void fixture operand | the operand's side effect identical (it writes a per-point record the test reads back) |
| init section (Q9) | `probe_init.sl` (a parameter default calls the operand) | 0 differing pixels; thread index 0 |
| per-thread operand, 8 threads | the 1.1 fixture under `.slo`, the scene of `ShadeopAbi_Versions_Render` | 8 distinct data pointers, 8 cleanups, as the `.rslo` run reports |
| missing operand at render time | `probe.slo` with the module removed from the search path | the Q2 warning; the `.rslo` fallback then prints the interpreter's "Failed to locate DSO function"; no crash |
| compile coverage | `LibShader_GateHardening` positive case | `oshader --jit probe.sl` exits 0; the module declares `op_dso_call` |

Before the tests are relied on, research R14a-c are measured; a defect found there is reported to
the maintainer, not reproduced.
