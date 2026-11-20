# Contract: `IShaderBackend`, the description/payload split and execution dispatch

Headers: `src/libshader/shading/shaderBackend.h`, `shaderPayload.h`, `shader.h`. Internal C++ (FR-023). Implements
FR-002, FR-007, FR-009, FR-010, FR-020. Decisions: research.md R2, R3, R4, R6.

## IShaderBackend

```cpp
class IShaderBackend {
    public:
        virtual ~IShaderBackend() = default;
        virtual const char *key() const = 0;         // "rslo", "slo"
        virtual const char *extension() const = 0;   // passed to IShaderFileResolver::locate
        // A description whose backend is this and whose payload this backend owns, or nullptr with the format's own
        // diagnostics (the file is then left to the next backend in the order)
        virtual CShader *load(const char *shaderName, const char *path) const = 0;
        virtual CShaderInstance *instantiate(CShader *desc, CAttributes *attributes, CXform *xform) const = 0;
        // The bind-time init of the instance's parameter defaults; the facade wraps it in arenaBegin/End(globalArena)
        virtual void initialise(CShaderInstance &instance) const = 0;
        // Run the instance for the context's current batch (varying, tags, lights as today)
        virtual void execute(CShadingContext &context, CShaderInstance &instance, float **locals) const = 0;
};

void registerShaderBackend(const IShaderBackend *backend, int priority);
void registerUnavailableShaderFormat(const char *key, const char *warningFormat); // warningFormat takes the shader name
void unregisterShaderBackend(const char *key);                                    // tests only
```

No member names OSL, a closure, an LLVM type or a bytecode type (FR-030). Metadata is not a member: readers live in
`libRSLInfo` (contracts/shader-metadata.md, research.md R15).

## The two built-ins

| | Interpreter (`rsloBackend.cpp`) | JIT (`sloBackend.cpp`) |
|---|---|---|
| `key()` / `extension()` | `"rslo"` | `"slo"` |
| priority | 0 | 10 |
| `load()` | `parseShader(name, path)` (`rslo.y`, unchanged signature) | `parseSloShader(name, path)` (`sloLoader.cpp`, unchanged signature) |
| `instantiate()` | `new CProgrammableShaderInstance(desc, attributes, xform)` | the same |
| `initialise()` | `rsloInit.cpp`: today's `CRendererContext::init` interpreter body (`init.cpp:254-426`), hooks below | today's JIT body (`init.cpp:209-252`), resolver below |
| `execute()` | `context.interpret(&instance, locals)` | today's JIT block (`execute.cpp:502-528`), verbatim |
| JIT-less build | registered | not compiled; `registerUnavailableShaderFormat("slo", <today's text>)` |

Both register through a static registrar in their own translation unit (`libshader_shading`, renamed `libRSLRuntime` in Phase 8, is shared, so the
registrars always run).

## The split (FR-007)

`CShader` keeps `name`, `type`, `parameters`, `usedParameters`, `flags`, `data` and gains `backend`, `payload`
(data-model.md). `parseShader` (`shaderCreate` in `rslo.y`) builds a `CRsloPayload`; `parseSloShader` a `CSloPayload`;
each sets `desc->backend` to its backend's singleton, so a description built outside a `ShadingSystem` (the contract
harness) runs through the same dispatch. `CShader::analyse()` becomes a load-time helper taking the frame layout's
`numGlobals` (the light flags and `CLightShaderData` indices come out identical). `shader.h` no longer includes
`llvmJit.h`; `TShaderJitEntry` appears only in the JIT backend's files.

Readers after the split:

| Field | Read by |
|---|---|
| description fields | everyone (core, services, Shading, tests) |
| `CRslPayload` (`numVariables`, `varyingSizes`) | `CProgrammableShaderInstance::prepare()`, both RSL backends |
| `CRsloPayload` (`codeArea`, `constantEntries`, `codeEntryPoint`, `initEntryPoint`) | `interpret()`, `rsloInit.cpp`, `rslo.y` |
| `CSloPayload` (`jitEntry`, `jitInitEntry`) | `sloBackend.cpp`, `sloLoader.cpp` |

## Execution dispatch (FR-010)

```text
CProgrammableShaderInstance::execute(ctx, locals)    -> ctx->execute(this, locals)        (unchanged)
CProgrammableShaderInstance::illuminate(ctx, locals) -> ctx->execute(this, locals)        (unchanged)
CShadingContext::execute(CShaderInstance *, locals)  -> instance->shaderDescription()->backend->execute(*this, *instance, locals)
CRsloBackend::execute                                 -> ctx.interpret(static_cast<CProgrammableShaderInstance *>(&instance), locals)
                                                         (new public member; today's loop, no JIT code)
CSloBackend::execute                                  -> the moved JIT block
```

`CShadingContext::execute` takes the neutral `CShaderInstance *` (today `CProgrammableShaderInstance *`): every
existing caller passes a `CProgrammableShaderInstance *`, which converts implicitly, so no caller changes; a test
backend's own instance class goes through the same entry. `CShaderInstance` gains
`virtual const CShader *shaderDescription() const = 0` (the RSL instance returns `parent`).

**Invariant (why this is today's switch)**: `jitEntry` gets a non-null value only in `sloLoader.cpp`; `shader.cpp` only
nulls or copies it; `parseSloShader` sets `codeArea = nullptr`. Hence "`jitEntry != nullptr`" is "loaded by
`parseSloShader` and compiled", and "`codeArea == nullptr && jitEntry == nullptr`" is "loaded by `parseSloShader`, JIT
compile failed". The JIT backend therefore keeps both cases: the call, or the `CODE_SYSTEM` warning
"Shader \"%s\" has no bytecode and no JIT entry — skipping".

**Kept around the JIT call, verbatim**: `currentShadingState->currentShaderInstance = instance` (set before both paths
today), the `stuff[]` arrays (`constantEntries` is null for a `.slo`), the save/set/restore of
`libshader::activeContext()` (nested light, entry point (6) of the light contract), the `[JIT-PROBE]`/`[JIT-DBG]` debug
logs, and the light's ambient-`Cl` accumulation into `alights->savedState[1]` when `PARAMETER_NONAMBIENT` is unused.

`RSLShading::shade()` keeps calling `ctx->execute()`: same effect for both formats (A-6). No new `friend` of
`CShadingContext` (`check_scope.cmake` S1c).

## Bind-time init (FR-009, #134)

Interpreter (`rsloInit.cpp`, a free function of the backend; `DSO_THREAD_INDEX 0` and the `initDraw` stream kept):

| Hook in the macro bodies | Before (core) | After (Shading) |
|---|---|---|
| `findCoordinateSystem(name, from, to, kind)` | `CRenderer::findCoordinateSystem` | `libshader::bindCoordinateSystem(name, instance, from, to, kind)` |
| global parameter relink | `CRenderer::retrieveVariable(name)` | `libshader::lookupVariable(name)` (same table through the services) |
| `threadMemory` | `globalArena.top` | `globalArena.top` |
| `scripterror` | nullifies `codeEntryPoint`/`initEntryPoint` of the parent | the same fields, now in the `CRsloPayload` |

JIT (`CSloBackend::initialise`): the same body; `jitSetInitXform(instance.xform)` unchanged; the resolver passed to
`jitSetInitCoordinateResolver()` calls `libshader::bindCoordinateSystem()`.

`libshader::bindCoordinateSystem()` asks `getMatrix{name, no object, shader = instance, no light, 0, phase = Bind}`.
The renderer answers a `Bind` request with `CRenderer::findCoordinateSystem` alone, which is exactly what both inits use
today. Found: `from`/`to` as answered. Not found: identity, no warning (the JIT's present answer; the interpreter's is
undefined today) (approved, P-Q2). No services: identity, with the one "renderer services do not provide coordinate
systems" warning `findCoordinateSystem` gives.

Phase 1 records every named space at bind time under both backends (research.md R17) before this moves.

## Test backend (SC-003)

`tests/shading_system/test_backend.cpp` registers a backend with key and extension `"tst"`; its `load()` reads a
one-line text file (shader name, type, a colour), its `instantiate()` makes a `CShaderInstance` subclass of its own whose
`execute()` writes the colour into `Ci`, and its `initialise()` counts calls. The test builds a `ShadingSystem` with a
resolver double and a services double, requests the name with preference `"tst"`, checks it was found, loaded once,
cached, bound (key taken, init run once), and executed through `CShadingContext::execute()`, with no file outside
`src/libshader/` and the test edited.
