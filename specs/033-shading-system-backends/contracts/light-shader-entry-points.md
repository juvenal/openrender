# Contract: light-shader entry points before and after spec 033 (FR-029)

**For**: spec 057 (area-light geometry), which merges master after 033 lands and adapts to it. 033 **moves** the
light-shader path and does not redesign it: images, diagnostics and the order of every step stay as they are. This
record was written against master `278b0394` only; nothing from spec 057's branch was read.

**Legend**: "Before" is master `278b0394` (file:line). "After" is the tree at the end of spec 033 (file and function;
new files are under `src/libshader/shading/` unless a path says otherwise). Line numbers after the change are not
given: they move during implementation.

**Final check (T142, 2026-11-17).** Every "After" cell was checked against the final code: the functions and files named exist (`ShadingSystem::bind/find`, `CShaderSearchPathResolver`, `IShaderBindHost::registerLight`/`takeInstanceKey`, `rsloInit.cpp`, `CSloBackend::initialise/execute` in `sloBackend.cpp`, `bindCoordinateSystem` in `shaderBind.cpp`, `lightInstanceOf() -> CShaderInstance *`, `op_lightsource_f` with `destBytes = 64`); `git diff 278b0394 -- src/ri/render/rendererContext.cpp` has two hunks only (the includes, and `getShader` with the new `registerLight`), so `RiLightSourceV`, `RiAreaLightSourceV`, `RiIlluminate`, `extractLightHandle`, `takeInstanceKey` and `pendingInstanceKey` are unchanged and the leak is moved, not fixed.

## Library names (maintainer, 2026-11-14)

The light-shader path lives in the shading engine library, which spec 033 renames from `libshader_shading` to
**`libRSLRuntime`** (target `libRSLRuntime`, `openRender::RSLRuntime`, `libRSLRuntime.{dylib,so}`) in Phase 8; the old
target (`libshader_shading`), exported (`openRender::shader_shading`) and file names resolve for one release. Every file
cited below is in that library, and the loaders (`rslo.y`, `sloLoader.cpp`) are in it too. `libshader_runtime` (the
`.rslo` inspection reader used by `sloinfo`, renamed **`libRSLInfo`** in Phase 8 together with the metadata API) is a different, separate library, never linked together with `libRSLRuntime`, and is not on the light path. Renderer
services after 033: **2.4.0**; spec 057 takes 2.5.0 at its sync.

## The bind order (unchanged; 057 should rely on it)

For every `LightSource`/`AreaLightSource` (and every other shader type), binding runs these steps in this order, before
and after 033:

1. `"null"` returns no instance (the core, before anything is loaded);
2. find the description by name in the cache, else load it (format precedence, R2 of research.md);
3. not found: `CODE_NOSHADER` "Failed to find shader \"%s\"", no instance; wrong type: `CODE_NOSHADER`
   "Shader \"%s\" is not of the expected type", no instance. **No instance key is taken in either case**;
4. construct the instance (one reference);
5. take the instance key (`takeInstanceKey()`: the pending light-handle key if set, else the ordinal; it then clears
   the pending key and increments `shaderOrdinal`);
6. light only: `createCategories()`, which reads the **default** value of `__category` (the RIB parameters are not set
   yet), then registration into the frame's light list `CRenderer::allLights` (no extra reference);
7. the bind-time init, inside `arenaBegin(globalArena)` / `arenaEnd(globalArena)`;
8. the RIB parameters are set (`setParameters`);
9. back in `RiLightSourceV`/`RiAreaLightSourceV`: `attributes->addLight(instance)`; the instance is returned as the
   `RtLightHandle`.

## Entry points

### (1) Load and bind a `LightSource` / `AreaLightSource` shader

| | Before | After |
|---|---|---|
| RIB entry | `CRendererContext::RiLightSourceV` `rendererContext.cpp:2403`; `RiAreaLightSourceV` `:2430`; the default light at `RiWorldBegin` `:1020` | **unchanged** (same functions, same bodies) |
| Handle extraction | `extractLightHandle()` `rendererContext.cpp:2380`, writing `pendingInstanceKey` at `:2414` and `:2440` | **unchanged** (same function, same calls, same member) |
| Bind call | `CRendererContext::getShader(name, SL_LIGHTSOURCE, n, tokens, params)` `rendererContext.cpp:298-350` | `CRendererContext::getShader(...)` (same file, same signature, same callers). Body: the `"null"` test; a `ShaderRequest` with the attribute's and the option's `shaderformat` values and a `CShaderSearchPathResolver` over `currentOptions->shaderPath`; `ShaderGroupBuilder` `begin()` / `shader(...)` / `end()` (contracts/shader-group.md) |
| Load and cache | `CRenderer::getShader(name, path, preference)` `rendererFiles.cpp:372-439` (cache `globalFiles`; `parseShader`/`parseSloShader`); preference computed at `rendererContext.cpp:304-307` | `ShadingSystem::bind()` -> `ShadingSystem::find()` (`shadingSystem.cpp`): cache, `resolvePreference()`, the registered backends' `load()` (`rsloBackend.cpp` -> `parseShader` in `rslo.y`; `sloBackend.cpp` -> `parseSloShader` in `sloLoader.cpp`). `CRenderer::getShader(name, path, preference)` stays as a core forwarder to `ShadingSystem::find()` (used by approved tests) |
| Instance creation | `new CProgrammableShaderInstance(cShader, currentAttributes, currentXform)` `rendererContext.cpp:319` | `IShaderBackend::instantiate()` called by `ShadingSystem::bind()`; for both RSL backends still `new CProgrammableShaderInstance(desc, attributes, xform)` |
| Instance key | `instance->instanceKey = takeInstanceKey()` `rendererContext.cpp:320` (`takeInstanceKey` `:2369`) | `ShadingSystem::bind()` calls `IShaderBindHost::takeInstanceKey()`, which `CRendererContext` implements by calling its unchanged `takeInstanceKey()` |

**Known leak, moved not fixed.** The light's `pendingInstanceKey` is stored by `extractLightHandle`
(`rendererContext.cpp` ~:2414 and ~:2440) and cleared only by `takeInstanceKey()`. After a `"null"`, missing or
wrong-type light shader, `takeInstanceKey()` is never called, so the key stays set and the next instance created takes
it. Spec 033 keeps that code and that ordering exactly as they are, leak included, and does not fix it. The `"null"` test
stays in the core before the facade; the missing and wrong-type cases return from `ShadingSystem::bind()` before
`takeInstanceKey()` is called. **Spec 057 owns the fix and its test.**

### (2) `createCategories()` and the registration into the frame's light list

| | Before | After |
|---|---|---|
| Categories | `instance->createCategories()` `rendererContext.cpp:324` -> `CShaderInstance::createCategories` `shader.cpp:208` (reads the default `__category`, IDs through `libshader::parseCategories`, `serviceLookup.cpp:82`) | `ShadingSystem::bind()` step 6 calls the same `CShaderInstance::createCategories()` (unchanged, `shader.cpp`) |
| Light list | `CRenderer::allLights->push(instance)` `rendererContext.cpp:327` | `ShadingSystem::bind()` calls `IShaderBindHost::registerLight(instance)`; `CRendererContext::registerLight()` (`rendererContext.cpp`) does `CRenderer::allLights->push(instance)` |
| Readers of the light list | `photon.cpp:142-213` (photon emission runs every light from `CRenderer::allLights` with `prepare()` and `illuminate()`); `renderer.cpp:150` (release) | **unchanged** |

### (3) The light's bind-time init

| | Before | After |
|---|---|---|
| Call | `arenaBegin(globalArena); init(instance); arenaEnd(globalArena);` `rendererContext.cpp:333-335` | `ShadingSystem::bind()` step 7: `arenaBegin(globalArena); backend->initialise(*instance); arenaEnd(globalArena);` |
| Interpreter init | `CRendererContext::init()` `init.cpp:254-426` (the macro bodies of `scriptOpcodes.h`/`scriptFunctions.h` instantiated in the core, `findCoordinateSystem` -> `CRenderer::findCoordinateSystem` (19 sites), `CRenderer::retrieveVariable` for a global parameter) | `rsloInit.cpp` (the same body, moved with `git mv`, now a function of the interpreter backend). Hooks: `findCoordinateSystem` -> `libshader::bindCoordinateSystem()` -> `getMatrix{phase = Bind}`; `retrieveVariable` -> `libshader::lookupVariable()`. Same results bit for bit (research.md R6) |
| JIT init | `init.cpp:209-252`: `jitInitEntry` with `jitSetInitXform(instance->xform)` and `jitSetInitCoordinateResolver(initCoordinateResolver)` (`CRenderer::findCoordinateSystem`) | `CSloBackend::initialise()` (`sloBackend.cpp`): the same body; the resolver it installs calls `libshader::bindCoordinateSystem()`; `jitSetInitXform`/`jitSetInitCoordinateResolver` stay in `rslOps.cpp` |
| `init.cpp` | `src/ri/render/init.cpp` (426 lines, core) | gone from the core |

### (4) Running a bound light from a surface's `illuminance` / `diffuse()` / `specular()` / `ambient()`, and its output

| | Before | After |
|---|---|---|
| Light iteration | `CShadingContext::iterateLights()` `shading.cpp:2006-2074` (interpreter macro `runLightsTemplate` `execute.cpp:432`, the JIT's `rsl_run_lights`/`setupIlluminance`/`prepareDiffuse`/`callSpecular`/`callPhong`); ambient: the loop at `shading.cpp` ~:2105-2118 and `prepareAmbient()` `:2313-2348` | **unchanged** (same functions) |
| Active-light entry | `lightInstanceOf(const CActiveLight &)` `shaderRefs.h:43`, returns `CProgrammableShaderInstance *` (an unchecked downcast) | `lightInstanceOf()` (same file) returns **`CShaderInstance *`** (no downcast; every member the callers use, `categories`, `flags`, `prepare`, `illuminate`, `requiredParameters`, is on the base). Local variables in `iterateLights` and the ambient loops change type accordingly |
| Run | `light->prepare(shaderStateMemory, varying, n)` then `light->illuminate(this, locals)` -> `CProgrammableShaderInstance::illuminate` `shader.cpp:633` -> `CShadingContext::execute(instance, locals)` `execute.cpp:177` (JIT dispatch inside, `:502-528`) | the same two calls -> `CProgrammableShaderInstance::illuminate` -> `CShadingContext::execute()` (now the dispatcher) -> `IShaderBackend::execute()` -> interpreter: `CShadingContext::interpret()` (`execute.cpp`, today's loop without the JIT block); JIT: `CSloBackend::execute()` (`sloBackend.cpp`) |
| `currentLightInstance` | set before each light runs (`shading.cpp:2064`, ~:2110, `:2337`) | unchanged |
| Output: `L`, `Cl` | saved per light into `CShadedLight::savedState[0]`/`[1]` with `lightTags` by the interpreter's `saveLighting` (`execute.cpp:56`) and the JIT's `jitIlluminateEnd`/`jitSolarEnd` (`shading.cpp:2488`, `:2641`); copied into `varying[L]`/`varying[CL]` for each iterated light by `enterLight()` (`shading.cpp:2721`) | unchanged |
| Output: `__nondiffuse`, `__nonspecular` | `CLightShaderData` indices set at load by `CShader::analyse()` (`shader.cpp:115-162`), read as `savedState[2 + index]` (`shaderFunctions.h:833, 908, 988, 1069`; `shading.cpp:2215, 2281`) | unchanged indices and reads; `analyse()` becomes a load-time helper the two RSL loaders call (it needs the frame layout's `numGlobals`, now in the payload) |
| Output: ambient | the interpreter adds `Cl` into `alights->savedState[1]` at `execEnd` (`execute.cpp` ~:757-770); the JIT block does the same after the JIT call (`execute.cpp:517-527`) | interpreter: unchanged (`interpret()`'s `execEnd`); JIT: the same lines, moved verbatim into `CSloBackend::execute()` |

### (5) The JIT `lightsource()` lookup

| | Before | After |
|---|---|---|
| Entry | `op_lightsource_f` `rslOps.cpp:921-962`: the last light that ran (`currentShadingState->currentLightInstance`), its `getParameterShape()` (refuse above 64 bytes, one warning), then `getMessage{Light, objectHandle, shaderHandle(light), name, Float, scratch, wantVariable = false}`; returns `scratch[0]` | `op_lightsource_f` (same file, same signature): the same light handle and scratch; one request `getMessage{Light, ..., destBytes = 64}`; a `TypeMismatch` answer gives the same warning text and result 0; **no `getParameterShape()` call** (renderer services 2.4.0, approved P-Q1). Output byte-identical; #115 (last light that ran, floats only, the value returned) stays open |
| Interpreter (for comparison) | `CShadingContext::lightParameter` `shading.cpp:1916` (the iterated light, `wantVariable = true`) | unchanged |

### (6) The context save around a nested light's JIT call

| | Before | After |
|---|---|---|
| Where | `CShadingContext::execute` `execute.cpp:505-513`: `savedCtx = libshader::activeContext(); setActiveContext(this); jitEntry(...); setActiveContext(savedCtx);` (a surface's JIT reaching a light's JIT through `callDiffuse`/`callAmbient` -> `illuminate()` -> `execute()`) | `CSloBackend::execute()` (`sloBackend.cpp`): the same four lines, moved verbatim. The nesting path is unchanged: the light's `illuminate()` -> `CShadingContext::execute()` -> `CSloBackend::execute()` |

## What 033 does not change on this path

`RiLightSourceV`/`RiAreaLightSourceV`/`RiIlluminate` bodies; `extractLightHandle`, `takeInstanceKey`,
`pendingInstanceKey`; `CRenderer::allLights` and its readers (`photon.cpp`, `renderer.cpp`); `iterateLights`' logic,
`lightSideN`, `enterLight`/`exitLight`; the light shader's globals; every `op_*` signature; the `.slo` and `.rslo`
formats. Images stay byte-identical (SC-010: the light scenes of the suite, `-slo` and `-rslo`, area lights included).
