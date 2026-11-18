# Contract: `ShadingSystem`, the request, the resolver and the bind host

Header: `src/libshader/shading/shadingSystem.h` (and `shaderBackend.h` for the shared types). Internal C++ (FR-023).
Implements FR-001, FR-004, FR-005, FR-006, FR-008. Decisions: research.md R1, R2, R8, R9, R10.

## Types (sketch; names are binding, layout is not)

```cpp
namespace openrender::shading {

    inline constexpr int kAnyShaderType = -1;   // no type check (the CRenderer::getShader forwarder)

    struct ShaderRequest {
        const char *name;               // as the statement gave it
        int type;                       // SL_SURFACE ... SL_IMAGER, or kAnyShaderType
        const char *attributeFormat;    // CAttributes::shaderFormat, may be null
        const char *optionFormat;       // COptions::defaultShaderFormat, may be null
        const IShaderFileResolver *resolver;
    };

    // Shading owns it; the renderer core implements it over the current shader search path (one per request)
    class IShaderFileResolver {
        public:
            virtual ~IShaderFileResolver() = default;
            // Writes the path of `name` with `extension` into result (OS_MAX_PATH_LENGTH) and returns TRUE when found.
            // Same rules as CRenderer::locateFileEx: a name holding an extension is used as written.
            virtual int locate(char *result, const char *name, const char *extension) const = 0;
    };

    // Shading owns it; CRendererContext implements it
    class IShaderBindHost {
        public:
            virtual ~IShaderBindHost() = default;
            virtual uint64_t takeInstanceKey() = 0;               // called once per successful bind, after the type check
            virtual void registerLight(CShaderInstance *light) = 0; // light shaders only, after createCategories()
    };

    class ShadingSystem {
        public:
            ShadingSystem();    // takes the entries of the backend registry
            ~ShadingSystem();   // destroys the cache and every description

            // Find or load; no instance. nullptr when no registered backend found and loaded the name.
            CShader *find(const ShaderRequest &request);

            // The whole binding of one layer (order below). nullptr with today's diagnostics on failure.
            CShaderInstance *bind(const ShaderRequest &request, CAttributes *attributes, CXform *xform, IShaderBindHost &host);

            // attribute, else option, else OPENRENDER_DEFAULT_FORMAT
            static const char *resolvePreference(const char *attributeFormat, const char *optionFormat);
    };
}
```

The group builder (contracts/shader-group.md) calls `bind()` and then `setParameters()`.

## Find (FR-005, FR-006)

```text
lock cacheMutex
if cache has request.name                  -> return it (whatever the preference says now: #167, kept)
preference = resolvePreference(attributeFormat, optionFormat)
order      = [entry whose key == preference, if registered] + the other entries by priority
for entry in order:
    if entry is unavailable:               -> if entry is the preferred one: warning(CODE_INCAPABLE, entry.text, name); continue
    if resolver->locate(path, name, entry.extension) and (desc = entry.backend->load(name, path)) != null:
        log_debug("getShader: loaded shader '{}' from '{}'", name, path)
        cache.insert(desc->name, desc)     // the description's own copy, never request.name (arena aliasing)
        return desc
return null
```

Equivalence with `CRenderer::getShader` (`rendererFiles.cpp:372-439`): with the two built-ins registered (rslo 0, slo 10)
the order is slo, rslo for preference `"slo"` and rslo, slo for every other value; in a JIT-less build `"slo"` is an
unavailable entry whose text is today's (`"Shader \"%s\": shaderformat \"slo\" requested, but this build has no LLVM JIT
support; using the .rslo interpreter instead\n"`), so the warning fires once per uncached request when `"slo"` is
preferred and never otherwise. A file found but refused by its loader (an `.slo` with no metadata, an `.slo` naming a
DSO operand the session lacks, an `.rslo` that fails to parse) falls through to the next entry with the loader's own
diagnostics, as today.

## Bind (FR-008; order recorded in contracts/light-shader-entry-points.md)

1. `desc = find(request)`; null: `error(CODE_NOSHADER, "Failed to find shader \"%s\"\n", name)`, return null.
2. `request.type != kAnyShaderType && desc->type != request.type`:
   `error(CODE_NOSHADER, "Shader \"%s\" is not of the expected type\n", name)`, return null.
3. `log_info("getShader: loading shader '{}', type '{}', in format '{}'", name, desc->type, preference)`.
4. `instance = desc->backend->instantiate(desc, attributes, xform)`.
5. `instance->instanceKey = host.takeInstanceKey()`.
6. `desc->type == SL_LIGHTSOURCE`: `instance->createCategories(); host.registerLight(instance);`
7. `arenaBegin(globalArena); desc->backend->initialise(*instance); arenaEnd(globalArena);`
8. return `instance` (one reference).

The cache lock is released before step 2; `initialise()` never runs under it.

## The core's side

- `CShaderSearchPathResolver` (`src/ri/render/searchPathResolver.h`): `locate()` is
  `CRenderer::locateFileEx(result, name, extension, path)`, built on the stack in `CRendererContext::getShader` over
  `currentOptions->shaderPath`.
- `CRendererContext` implements `IShaderBindHost` (`takeInstanceKey()` unchanged; `registerLight()` pushes into
  `CRenderer::allLights`).
- `CRenderer::shadingSystem` replaces `CRenderer::globalFiles`: `new ShadingSystem` in `initFiles()`, `delete` in
  `shutdownFiles()`.
- `CRenderer::getShader(name, path)` and `CRenderer::getShader(name, path, preferredType)` stay, as forwarders:
  `ShaderRequest{name, kAnyShaderType, preferredType, nullptr, &resolver}` -> `shadingSystem->find()`. They name no
  backend (approved tests call them, research.md R14).
- `rendererFiles.cpp` loses the `parseShader` declaration and the `sloLoader.h` include; `renderer.h` loses `globalFiles`.

## Threading

`find()` and `bind()` are called by whoever runs RIB statements: the RIB thread before rendering, and render threads
expanding procedurals or delayed archives under `CRenderer::delayedMutex`. `cacheMutex` serializes find/load/insert
(the `.rslo` parser is not reentrant). Nothing in the facade takes `CRenderer::shaderMutex` (gotcha 25).
