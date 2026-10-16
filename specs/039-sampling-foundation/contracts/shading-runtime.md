# Contract: Shading runtime identity and call sites (Shading domain)

This covers the compiler, the `.rslo` format, the JIT runtime ABI and `CShadingState`. The design is in research §R3.5–§R3.8.

## Compiler (`src/libshader/compiler/`)

- **C1**: after the last IR pass, the compiler assigns a `callSite` ordinal (`uint16`, in IR order, starting at 0 per shader) to every stochastic builtin call:
  - `random`, `urandom`;
  - `trace`, `transmission`, `visibility`, `gather`, `occlusion`, `indirectdiffuse`;
  - `texture`/`shadow`/`environment` when they take stochastic filter samples;
  - light iteration in `illuminance`/`solar`/`illuminate`.
- **C2**: `random`/`urandom` are never pure. CSE, DCE-of-duplicates and `UniformLifting` must not merge two of these calls or hoist one out of a loop. This is tested.
- **C3**: the same source gives the same ordinal list in `.rslo` and in `.slo`. Tested by compiling both ways and comparing (`LibShader_CallSiteOrdinals`).

## Interpreter (`.rslo`)

- **I1**: stochastic instructions carry the `callSite` as an extra operand. This is a format change:
  - the `.rslo` format version is bumped, so the loader rejects an older file with a diagnostic and never misreads it;
  - every shipped `.rslo` is rebuilt by the build.
- **I2**: `random()`/`urandom()` draws come from `get1D(substream(pointKeys[i], shaderKey, callSite, occurrence[callSite][i], depth), dim, …)`. The uniform form uses `batchKey` in place of `pointKeys[i]`.

## JIT runtime ABI (`rslOps.cpp`, `llvmEmitter.cpp`)

- **J1**: `op_random_f`, `op_random_v` and the trace, transmission, gather, occlusion, indirectdiffuse and texture-family entries gain a `uint32_t callSite` argument. The emitter passes the compiler's ordinal as a constant. Every `.slo` is rebuilt.
  - Each changed entry is **renamed** with a version suffix, for example `op_random_f_v2`.
  - A stale `.slo` then fails at JIT bind time with an unresolved symbol instead of reading garbage arguments.
- **J2**: per point, the JIT increments `occurrence` for active lanes only, exactly as the interpreter does. The comparison tests in SC-008 check this.
- **J3**: the changed symbols must still resolve at JIT bind time (CLAUDE.md gotcha 3). The `-slo` visual suite proves it.

- **J4**: derivative-offset tail vertices (`numVertices = 3·numRealVertices`) copy their real vertex's key and occurrence, so `Du(random())` and `Dv(random())` are 0 (§R3.10a). This is the same on both backends.

## CShadingState additions (`src/libshader/shading/shading.h`)

| Member | Filled by | When |
|---|---|---|
| `pointKeys[numVertices]` | Hiders' `trace.cpp` shading-group loop for ray hits (from `CRay::key`). `CShadingContext::shade()` for grid points (cell rule, after `object->sample()`). | Before the shaders run |
| `batchKey` | The same two sites (§R3.7). | Before the shaders run |
| `callSiteOccurrence` | The runtime (lazily allocated). | Reset per invocation |
| `currentShaderKey` | `execute()` | Per shader instance |

**Grid shading reads `object->stableId`**: an inline data-member read through `shading.cpp`'s existing include of `object.h` (DB-006). It adds no new include and no link symbol, so archcheck sees no change. Spec 029's `ShaderGlobals` takes this read over.

## Texturing entry points (`src/ri/texture/texture.cpp`)

- **T1**: the stochastic `lookup`/`shadow` paths that read `context->random2d/random4d` today take a `SubstreamKey` parameter instead. The caller (Shading) builds it. The direction is Shading → Texturing, which is allowed. Texturing then draws from Infra.

## Known limits (research §R10)

- **JIT optional parameters**: the JIT `trace`/`transmission`/`visibility`/`occlusion` paths ignore their optional parameter lists (`samples`, `samplecone`, ...). Interpreter and JIT bit-identity (FR-018, SC-008) is guaranteed only for the call forms both backends implement: plain `trace`/`transmission`/`visibility`/`occlusion`/`indirectdiffuse`, and `gather`. The gap is GitHub #55, fixed on its own `fix/` branch once 039 is complete.
- **GitHub #8**: SC-008's test shader avoids `if`-gated uniform assignments.
