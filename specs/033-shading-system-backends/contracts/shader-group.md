# Contract: the group-shaped API (one layer)

Header: `src/libshader/shading/shaderGroup.h`. Internal C++. Implements FR-011, FR-012. Decision: research.md R11
(**approved, P-Q5**: a one-layer group is its layer's instance).

## Binding

```cpp
class ShaderGroupBuilder {
    public:
        explicit ShaderGroupBuilder(ShadingSystem &system);
        void begin();
        // Bind one layer: ShadingSystem::bind() then setParameters(np, tokens, values). false when the layer was not
        // bound (its diagnostics were given) or was refused (a second layer).
        bool shader(const ShaderRequest &request, CAttributes *attributes, CXform *xform, IShaderBindHost &host,
                    int np, const char **tokens, const void **values);
        // Always refused before spec 034: CODE_INCAPABLE, returns false, changes nothing
        bool connect(const char *fromLayer, const char *fromParameter, const char *toLayer, const char *toParameter);
        // The bound group: its single layer's instance with one reference, or nullptr if no layer was bound
        CShaderInstance *end();
};
```

Diagnostic for `connect()` and for a second `shader()` (the refused layer is not loaded and not instantiated, the group
keeps its first layer): `error(CODE_INCAPABLE, "Shader group connections and multi-layer groups are not supported
before spec 034\n")`, once per call.

The core's `CRendererContext::getShader()` is the only production user:

```text
if name is "null"                     -> return NULL
CShaderSearchPathResolver resolver(currentOptions->shaderPath)
ShaderRequest request{name, type, currentAttributes->shaderFormat, currentOptions->defaultShaderFormat, &resolver}
ShaderGroupBuilder group(*CRenderer::shadingSystem)
group.begin(); group.shader(request, currentAttributes, currentXform, *this, np, params, vals)
return group.end()                    // adopted by the slot (ShaderGroupRef::adopt) or kept as the light handle
```

Every binding the renderer makes today goes through it: surface, displacement, atmosphere, interior, exterior,
light, area light, imager, and the default surface and light at `RiWorldBegin`. No RIB statement and no grammar change
(FR-012).

## Running a group for a batch

```cpp
namespace libshader {
    // Allocate the group's locals for numVertices points from memory and copy the parameter defaults
    float **groupPrepare(const CShaderGroupRefTarget *group, CMemPage *&memory, float **varying, int numVertices);
    // Run the group for the context's current batch
    void groupExecute(CShadingContext &context, const CShaderGroupRefTarget *group, float **locals);
}
```

The batch is the context's current shading state (`varying`, `tags`, counts), filled from spec 026's `ShaderGlobals`
by `shade()` as today. With one layer both forward to `shaderInstanceOf(group)->prepare()` / `->execute()`: the same
calls the sites make now, so the variables are bit-identical (US4 AS1). Sites converted: `shade()`'s surface,
displacement and atmosphere runs (`shading.cpp:1283-1303`, `:1533-1592`), the volume step (`shading.cpp:746-748`), and
the imager (`imager.cpp:62-63`). Light execution (`iterateLights`, the ambient loops, photon emission) keeps calling the
instance's `prepare()`/`illuminate()`: the light path is moved, not redesigned (FR-029).

## Seams for spec 034

A group object (`CShaderGroup : CShaderGroupRefTarget`) holding several layers replaces "the instance is the group"
behind `shaderInstanceOf()`, `groupPrepare()`/`groupExecute()` and `ShaderGroupBuilder`; `connect()` gains a body. No
caller of the builder or of the run helpers changes when it does.
