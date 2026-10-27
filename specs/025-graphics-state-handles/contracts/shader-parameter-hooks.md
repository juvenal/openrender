# Contract: Shader-parameter hooks

**Interface owner**: Graphics State (`src/ri/state/shaderParameterHooks.{h,cpp}`).
**Implementer**: Shading (`src/libshader/shading/shaderParameters.{h,cpp}`).
**Installer**: the renderer core (`src/ri/render/rendererShadingServices.cpp`).
**Requirements**: FR-005 to FR-007. The `variableLookup.h` precedent comes from spec 023.

## State side

```cpp
class CAttributes;
class CVariable;

struct SShaderParameterHooks {
    unsigned int (*usedParameters)(const CAttributes &);
    CVariable *(*findParameter)(const CAttributes &, const char *name);
};

// Installed and removed on one thread, with the renderer's lifetime. nullptr uninstalls.
void installShaderParameterHooks(const SShaderParameterHooks *hooks);
const SShaderParameterHooks *shaderParameterHooks();
```

`CAttributes` keeps both members:
- `checkParameters()` sets `usedParameters = hooks ? hooks->usedParameters(*this) : 0`.
- `findParameter(name)` returns `hooks ? hooks->findParameter(*this, name) : nullptr`.

The callers are not edited:
- State's `pl.cpp` (9 sites);
- Geometry's `blobby.cpp:304`;
- Scene Interface's `rib.y:235`.

`usedParameters` stays a stored field, read by Shading at `shading.cpp:904`.

## Shading side (moved verbatim from `attributes.cpp:397-452`)

```cpp
namespace libshader {
unsigned int usedShaderParameters(const CAttributes &);   // OR of requiredParameters() over
                                                          // displacement, surface, atmosphere, interior,
                                                          // exterior and every light; | PARAMETER_N
                                                          // when no surface is bound
CVariable *findShaderParameter(const CAttributes &, const char *name);
                                                          // first match in displacement, then surface,
                                                          // then atmosphere; nullptr otherwise
const SShaderParameterHooks &shaderParameterHooks();      // { &usedShaderParameters, &findShaderParameter }
}
```

The `bxdf` slot is ignored by both functions in 025 (FR-005, confirmed 2026-10-26).

**Precondition**: while the hook is installed, every slot target and every light target is a
`CShaderInstance`. The hook `static_cast`s them, as the code before 025 did implicitly.
- **Spec 034** must revisit this when non-instance group targets reach the slots.
- **Tests that bind State-only doubles** (the `State_*` State-only tests, `test_interior_exterior_selection`)
  must do so with no `RiBegin` session open, so that no hook is installed. In
  `test_interior_exterior_selection`, the double-binding cases already run before the CSG case
  that opens a session; keep that order or give them their own process.

## Lifetime and recompute points

- **Install**: `rendererShadingServicesInit()`. That is in `CRenderer::beginRenderer()`, after
  `initDeclarations()` and before `CRendererContext` constructs its first `CAttributes`.
- **Uninstall**: `rendererShadingServicesShutdown()`, last in `endRenderer()`.
- **Never** from `rendererDeclarations.cpp`: ribVector compiles that file and does not link
  libshader_shading.
- **Recompute points**, the same as today:
  - inside State: the constructor, `addLight`, `removeLight`, `restore(shading)`;
  - every `bind*` and `setDefaultSurface`, which replace the core's former separate
    `checkParameters()` calls one-for-one.
  - The copy constructor copies the stored value without recomputing, as today.
- **With no hook installed** (State-only programs, ribVector, ribOut): `0` and `nullptr`. No
  process that shades runs without the hook.

## Edges

- State → (function pointer): no include, no link edge.
- Core → Shading (`shaderParameters.h`) and Core → State: allowed.
- Shading → State (`attributes.h`, to read the slots and lights): allowed.
- No Geometry → Shading or State → Shading edge is added (FR-006).

## Equivalence test (FR-007)

`ShadingParity_ShaderParameterEquivalence` has two parts.

**Part A** is API-neutral and green on `dfffba0`:
- Fixtures: three shaders, each declaring `float shared` with a different default (displacement
  1, surface 2, atmosphere 3), plus one slot-unique parameter each; and a light.
- The test runs `RiBegin`, binds them through the Ri calls, and checks
  `getAttributes(FALSE)->findParameter`:
  - `shared` gives default 1 (displacement shadows the others);
  - each unique name is found;
  - an absent name gives `nullptr`.
- It also checks that `usedParameters` equals the OR of `CRenderer::getShader(...)->usedParameters`
  over the bound shaders and the light, `| PARAMETER_N` exactly when no surface is bound.
- It covers these binding sets:
  - none;
  - surface only;
  - all three;
  - after `removeLight`;
  - after `ResourceRestore`.
- **When each case is measured.** `RiWorldBegin` auto-binds `defaultsurface` when no `Surface` was
  given, and `defaultlight` when no `LightSource` was given.
  - "None" is measured **before** `RiWorldBegin` on the fresh `RiBegin` block, where only
    `PARAMETER_N` applies.
  - Every other case is measured inside `RiWorldBegin` … `AttributeBegin`. There the oracle ORs in
    `defaultsurface` when the case binds no surface, and `defaultlight` when no `LightSource`
    preceded `RiWorldBegin`.
  - The fixture file states which defaults each case sees.

**Part A was run on `dfffba0` on 2026-10-26**: 64/64 checks green; the precedence-swap mutation gave 4 failures (validation.md §0.4).

**Part B** is red until the code lands (a build failure): the same assertions made directly
against `libshader::usedShaderParameters`/`findShaderParameter`.

**Mutation check**, recorded in `validation.md`: swapping the displacement/surface precedence
turns both parts red.
