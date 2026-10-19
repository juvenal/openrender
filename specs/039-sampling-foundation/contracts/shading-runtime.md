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

## Format versions (`.rslo` and `.slo`)

- **V1**: the two shader formats are versioned separately, `RSLO_FORMAT_*` (`src/common/rsloFormat.h`) and `SLO_FORMAT_*` (`src/common/sloFormat.h`), both **2.0.0** after Phase 4. They follow semantic versioning:
  - **MAJOR**: a loader of another major cannot read the file (operand layout, JIT ABI or `op_*` signature changed). Older loaders cannot read newer files and newer loaders cannot read older ones.
  - **MINOR**: additive. A loader reads a file when `file.major == supported.major` and `file.minor <= supported.minor`.
  - **PATCH**: no effect on the format; never checked.
  - The rule lives in one place, `formatVersionReadable()` in `src/common/formatVersion.h`, used by both `.rslo` lexers (`runtime/rslo.l`, `shading/rslo.l`) and by the JIT loader.
- **V2**: a `.rslo` refusal names the file's format, the supported one and the remedy: `Version mismatch: this file is .rslo format 1.0.0, this renderer reads format 2.0.0 (same major, minor up to 0); recompile the shader with oshader`.
- **V3**: `oshader --jit` writes the `.slo` version as the named metadata node `openrender.slo.format`, one node of three `i32` operands (major, minor, patch). `openrender.shader.version` is unchanged (an informational string; parsed, not `stoi`d). The JIT loader (`CLLVMJitEngine::compileShader`) checks the node **before** it adds or links the module; a missing or unreadable node counts as 1.0.0. A refusal is `error(CODE_BADFILE, ...)` naming the shader, the file, the module's format, the supported one and `recompile it with oshader --jit`.
- **V4**: the `_v2` renaming (J1) and the link-failure diagnostic stay as a backstop for a module of the current format whose calls no longer resolve (`LibShader_StaleShaderRejectedJit`). A failed bind reports through the diagnostic sink, naming the shader and including LLVM's unresolved-symbol text.
- Tests: `LibShader_Format_*` (`test_format_versions.cpp`). `LibShader_Format_rslo_lower_minor` reports SKIPPED while the supported `.rslo` minor is 0.

## Interpreter (`.rslo`)

- **I1**: stochastic instructions carry the `callSite` as an extra, **last** operand (`.rslo` format **2.0**, `src/common/rsloFormat.h`); the loader strips it before it resolves the opcode and keeps it in `TCode::callSite`. This is a MAJOR format change (V1):
  - the `.rslo` major is bumped, so the loader rejects an older file with a diagnostic and never misreads it;
  - every shipped `.rslo` is rebuilt by the build.
- **I2**: `random()`/`urandom()` draws come from `get1D(substream(pointKeys[i], shaderKey, callSite, occurrence[callSite][i], depth), dim, …)`. The uniform form uses `batchKey` in place of `pointKeys[i]`.
  - **No source-level test for the uniform form.** The RSL compiler rejects assigning `random()`/`urandom()` to a uniform variable (`Can not assign to uniform variable`; also as a uniform parameter default), so the `batchKey` branch of the draw cannot be reached from RSL source, and no shader-level test (`SamplingJitParity_*`) exercises it. It is implemented as specified and covered by review only, unless a hand-built instruction stream is added later. (Phase 6 tests found this; see research R3.7.)

## JIT runtime ABI (`rslOps.cpp`, `llvmEmitter.cpp`)

- **J1**: `op_random_f`, `op_random_v` and the trace, transmission, visibility, gather (`op_gatherHeader`), occlusion, indirectdiffuse, illuminance (`op_illuminance_begin`) and texture-family (texture, environment, shadow) entries gain a `uint32_t callSite` argument, always the **last** one. The emitter passes the compiler's ordinal as a constant. Every `.slo` is rebuilt, and the `.slo` format major is bumped (V1, V3).
  - Each changed entry is **renamed** with a version suffix, for example `op_random_f_v2`.
  - A stale `.slo` is refused by the format check (V3) before it is linked; if it somehow passes, it fails at JIT bind time with an unresolved symbol instead of reading garbage arguments.
- **J1a**: `solar` and `illuminate` carry no ordinal: they declare a light's own emission and draw nothing at that point. (C1 listed them; revisit if a light needs a stream of its own.)
- **J2**: per point, the JIT increments `occurrence` for active lanes only, exactly as the interpreter does. The comparison tests in SC-008 check this.
- **J3**: the changed symbols must still resolve at JIT bind time (CLAUDE.md gotcha 3). The `-slo` visual suite proves it.

- **J4**: derivative-offset tail vertices (`numVertices = 3·numRealVertices`) copy their real vertex's key and occurrence, so `Du(random())` and `Dv(random())` are 0 (§R3.10a). This is the same on both backends. It holds for **ray-hit shading only** (the raytrace hider, and secondary-ray hits): a reyes grid takes `Du()`/`Dv()` from its neighbouring grid vertices, which draw independent values, so `Du(random())` is not 0 there. `SamplingJitParity_RandomProbe_*` checks all three channels under the raytrace hider and `_RandomProbeReyes_*` only the distinctness and range channels.

## Test hook (Phase 4)

`CShadingContext::pointKeyObserver` (`shading.h`) is a null-in-production function pointer. When set, `shade()` calls it once per shaded grid, after the per-point keys are filled, with a `CPointKeyReport {surface, numVertices, keys, u, v}`. Keys are filled only when the frame's sampler is not `Legacy`. It exists for `Sampling_ObjectIds_*` and `Sampling_CellKeyUniqueness_*`; nothing else may use it.

## CShadingState additions (`src/libshader/shading/shading.h`)

| Member | Filled by | When |
|---|---|---|
| `pointKeys[numVertices]` | Hiders' `trace.cpp` shading-group loop for ray hits (from `CRay::key`). `CShadingContext::shade()` for grid points (cell rule, after `object->sample()`). | Before the shaders run |
| `batchKey` | The same two sites (§R3.7). | Before the shaders run |
| `callSiteOccurrence` | The runtime (lazily allocated). | Reset per invocation |
| `currentShaderKey` | `execute()` | Per shader instance |

**Grid shading reads `object->stableId`**: an inline data-member read through `shading.cpp`'s existing include of `object.h` (DB-006). It adds no new include and no link symbol, so archcheck sees no change. Spec 029's `ShaderGlobals` takes this read over.

## Texturing entry points (`src/ri/texture/texture.cpp`)

- **T1**: the stochastic `lookup`/`shadow` paths that read `context->random2d/random4d` today draw from the context's **draw cursor** instead (`context->drawNext2()`/`drawNext4()`). Shading binds the cursor to the sub-stream of the lookup's point, shader, call site and occurrence (`bindDraws()`, the `DRAW_BIND` macro and the JIT `jitTexture*`/`jitEnvironment*`/`jitShadow*` entries) before every lookup, so no signature of Texturing changes. (As written, a `SubstreamKey` parameter on each `lookup` was planned; the cursor reaches the same sub-stream without touching the `CTexture`, `CEnvironment` and `CShadow` interfaces and the cache lookups that share the mechanism. Recorded as a deviation in tasks.md T073.)

## Draws in Shading (Phase 6)

- **D1**: `CShadingContext` offers one set of draw members (`shading.h`): under the Legacy sampler they forward to the context's own generators, otherwise they are `get1D`/`get2D` draws from `substream(point, shaderKey, callSite, occurrence, depth, samplesPerCall)`. The shader key is a hash of the shader's name, the same for `.rslo` and `.slo`. Nothing outside `shading.h` names the Legacy generators.
  - `drawRandom()` is `random()`/`urandom()`; `drawJitter4()`, `drawCone()` and `drawTime()` are the ray-sample draws (slot 0 and 1 the origin and direction jitter, 2 the cone direction, 3 the time; the sample index is the index in the sub-stream); `bindDraws()` plus `drawNext()` serve the code that cannot carry a sub-stream (the texture lookups, the irradiance cache, the hardcoded lights and the photons).
- **D2**: occurrence counters are per (shader, call site), one per real vertex, reset at the start of every shading invocation (`fillPointKeys()`) and, for the photon hider, every photon batch. A tail vertex takes its real vertex's occurrence.
- **D3**: a ray spawned by `trace()`/`transmission()`/`visibility()`/`gather()` and by `environment()`/`shadow()` in raytrace mode gets `childKey(point key, depth + 1, callSite, occurrence, sample index)` as its key; `trace(CRay*)` draws the importance of every ray that has no camera importance from the ray's own key.
- **D4**: a shader's init section (`init.cpp`) has no shading point: its `random()` draws from `CKeyedStream(shader name, call site, number of draws so far in this init run)`.
- **D5**: the radiance cache, the point hierarchy, `getDisp` and the BVH clustering draw from `CKeyedStream`s whatever the sampler (they are layout and cache choices, not sample points).

## Known limits (research §R10)

- **JIT optional parameters**: the JIT `trace`/`transmission`/`visibility`/`occlusion` paths ignore their optional parameter lists (`samples`, `samplecone`, ...). Interpreter and JIT bit-identity (FR-018, SC-008) is guaranteed only for the call forms both backends implement: plain `trace`/`transmission`/`visibility`/`occlusion`/`indirectdiffuse`, and `gather`. The gap is GitHub #55, fixed on its own `fix/` branch once 039 is complete.
- **GitHub #8**: SC-008's test shader avoids `if`-gated uniform assignments.
